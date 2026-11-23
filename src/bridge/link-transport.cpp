#include "link-transport.hpp"

#include <chrono>
#include <curl/curl.h>
#ifndef _WIN32
#include <sys/select.h>
#endif

namespace fiji::bridge {

namespace {

using Clock = std::chrono::steady_clock;

// Frames queued while the bot is slow are dropped beyond this many.
const size_t kMaxQueued = 500;
// Keeps the connection alive through proxies that close idle sockets.
const auto kPingEvery = std::chrono::seconds(30);

// Waits up to 100 ms for the socket to become readable (or writable).
void wait_socket(curl_socket_t sock, bool forWrite)
{
	fd_set set;
	FD_ZERO(&set);
	FD_SET(sock, &set);
	timeval timeout = {0, 100000};
	select((int)sock + 1, forWrite ? nullptr : &set, forWrite ? &set : nullptr, nullptr, &timeout);
}

bool send_all(CURL *curl, curl_socket_t sock, const char *data, size_t size, unsigned int flags,
	      const std::atomic<bool> &stopping)
{
	size_t offset = 0;
	int waits = 0;
	do {
		size_t sent = 0;
		const CURLcode res = curl_ws_send(curl, data + offset, size - offset, &sent, 0, flags);
		offset += sent;
		if (res == CURLE_AGAIN) {
			if (++waits > 100 || stopping) {
				return false;
			}
			wait_socket(sock, true);
		} else if (res != CURLE_OK) {
			return false;
		}
	} while (offset < size);
	return true;
}

} // namespace

LinkTransport::LinkTransport(Callbacks callbacks, std::string userAgent)
	: callbacks_(std::move(callbacks)),
	  userAgent_(std::move(userAgent))
{
}

LinkTransport::~LinkTransport()
{
	stop();
}

bool LinkTransport::validAddress(const QString &address)
{
	return address.startsWith(QStringLiteral("ws://")) || address.startsWith(QStringLiteral("wss://"));
}

void LinkTransport::start(const std::string &address)
{
	stop();
	stopping_ = false;
	retry_ = true;
	worker_ = std::thread([this, address] { run(address); });
}

void LinkTransport::stop()
{
	stopping_ = true;
	if (worker_.joinable()) {
		worker_.join(); // commands go through the dispatcher, so the worker never waits on the UI thread
	}
	std::lock_guard<std::mutex> lock(mutex_);
	outbox_.clear();
}

void LinkTransport::stopRetrying()
{
	retry_ = false;
}

void LinkTransport::send(const QByteArray &frame)
{
	if (!open_) {
		return;
	}
	std::lock_guard<std::mutex> lock(mutex_);
	if (outbox_.size() < kMaxQueued) {
		outbox_.push_back(frame);
	}
}

void LinkTransport::session(void *handle)
{
	auto curl = static_cast<CURL *>(handle);
	curl_socket_t sock = CURL_SOCKET_BAD;
	if (curl_easy_getinfo(curl, CURLINFO_ACTIVESOCKET, &sock) != CURLE_OK || sock == CURL_SOCKET_BAD) {
		return;
	}
	QByteArray message;
	char buffer[4096];
	auto lastPing = Clock::now();

	while (!stopping_) {
		std::deque<QByteArray> pending;
		{
			std::lock_guard<std::mutex> lock(mutex_);
			pending.swap(outbox_);
		}
		for (const auto &text : pending) {
			if (!send_all(curl, sock, text.constData(), size_t(text.size()), CURLWS_TEXT, stopping_)) {
				return;
			}
		}
		if (Clock::now() - lastPing > kPingEvery) {
			if (!send_all(curl, sock, "", 0, CURLWS_PING, stopping_)) {
				return;
			}
			lastPing = Clock::now();
		}
		for (;;) {
			size_t received = 0;
			const struct curl_ws_frame *meta = nullptr;
			const CURLcode res = curl_ws_recv(curl, buffer, sizeof(buffer), &received, &meta);
			if (res == CURLE_AGAIN) {
				break;
			}
			if (res != CURLE_OK || !meta || (meta->flags & CURLWS_CLOSE)) {
				return;
			}
			if (meta->flags & (CURLWS_PING | CURLWS_PONG | CURLWS_BINARY)) {
				continue;
			}
			message.append(buffer, qsizetype(received));
			// a message can arrive in several reads and in several fragments
			if (meta->bytesleft == 0 && !(meta->flags & CURLWS_CONT)) {
				if (callbacks_.frame) {
					callbacks_.frame(message);
				}
				message.clear();
			}
		}
		wait_socket(sock, false);
	}
}

void LinkTransport::run(std::string address)
{
	int wait = 2;
	while (!stopping_) {
		CURL *curl = curl_easy_init();
		if (!curl) {
			break;
		}
		curl_easy_setopt(curl, CURLOPT_URL, address.c_str());
		curl_easy_setopt(curl, CURLOPT_CONNECT_ONLY, 2L); // do the WebSocket handshake, then hand over
		curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
		curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
		curl_easy_setopt(curl, CURLOPT_USERAGENT, userAgent_.c_str());
		const CURLcode res = curl_easy_perform(curl);
		if (res == CURLE_OK) {
			wait = 2;
			{
				std::lock_guard<std::mutex> lock(mutex_);
				outbox_.clear();
			}
			open_ = true;
			if (callbacks_.log) {
				callbacks_.log(QStringLiteral("Bot link connected"));
			}
			if (callbacks_.opened) {
				callbacks_.opened(); // queues the HELLO
			}
			session(curl);
			open_ = false;
			if (callbacks_.log) {
				callbacks_.log(QStringLiteral("Bot link disconnected"));
			}
			if (callbacks_.closed) {
				callbacks_.closed();
			}
		} else if (callbacks_.attemptFailed) {
			callbacks_.attemptFailed(QString::fromUtf8(curl_easy_strerror(res)));
		}
		curl_easy_cleanup(curl);
		if (stopping_ || !retry_) {
			break;
		}
		// back off between attempts, but notice a stop quickly
		for (int i = 0; i < wait * 10 && !stopping_; i++) {
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
		wait = wait * 2 > 60 ? 60 : wait * 2;
	}
}

} // namespace fiji::bridge
