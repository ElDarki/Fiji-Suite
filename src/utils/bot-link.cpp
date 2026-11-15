#include "bot-link.hpp"

#include "../version.h"
#include "obs-websocket-api.h"
#include <atomic>
#include <chrono>
#include <curl/curl.h>
#include <deque>
#include <mutex>
#include <obs.hpp>
#include <QCoreApplication>
#include <thread>
#ifndef _WIN32
#include <sys/select.h>
#endif

extern obs_websocket_vendor vendor;

namespace {

using Clock = std::chrono::steady_clock;

// Events queued while the bot is slow or gone are dropped beyond this many.
const size_t maxQueued = 500;
// Keeps the connection alive through proxies that close idle sockets.
const auto pingEvery = std::chrono::seconds(30);

std::thread worker;
std::atomic<bool> stopping{false};
std::atomic<bool> finished{true};
std::atomic<BotLink::State> state{BotLink::State::Off};

std::mutex mutex; // guards outbox and lastError
std::deque<std::string> outbox;
std::string lastError;

void set_error(const std::string &error)
{
	std::lock_guard<std::mutex> lock(mutex);
	lastError = error;
}

void queue(const char *json)
{
	std::lock_guard<std::mutex> lock(mutex);
	if (outbox.size() < maxQueued) {
		outbox.emplace_back(json);
	}
}

// Waits up to 100 ms for the socket to become readable (or writable).
void wait_socket(curl_socket_t sock, bool forWrite)
{
	fd_set set;
	FD_ZERO(&set);
	FD_SET(sock, &set);
	timeval timeout = {0, 100000};
	select((int)sock + 1, forWrite ? nullptr : &set, forWrite ? &set : nullptr, nullptr, &timeout);
}

bool send_all(CURL *curl, curl_socket_t sock, const char *data, size_t size, unsigned int flags)
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

// {"type":"request","id":"7","request":"start_group","data":{...}} -> {"type":"response","id":"7","data":{...}}
void handle_message(const std::string &text)
{
	OBSDataAutoRelease message = obs_data_create_from_json(text.c_str());
	if (!message || strcmp(obs_data_get_string(message, "type"), "request") != 0) {
		return;
	}
	OBSDataAutoRelease request = obs_data_get_obj(message, "data");
	if (!request) {
		request = obs_data_create();
	}
	OBSDataAutoRelease response = obs_data_create();
	if (!fiji_call_request(obs_data_get_string(message, "request"), request, response)) {
		obs_data_set_string(response, "error", "unknown request");
		obs_data_set_bool(response, "success", false);
	}
	OBSDataAutoRelease reply = obs_data_create();
	obs_data_set_string(reply, "type", "response");
	obs_data_set_string(reply, "id", obs_data_get_string(message, "id"));
	obs_data_set_obj(reply, "data", response);
	queue(obs_data_get_json(reply));
}

// Runs one connection until it drops or the link is stopped.
void session(CURL *curl)
{
	curl_socket_t sock = CURL_SOCKET_BAD;
	if (curl_easy_getinfo(curl, CURLINFO_ACTIVESOCKET, &sock) != CURLE_OK || sock == CURL_SOCKET_BAD) {
		return;
	}
	std::string message;
	char buffer[4096];
	auto lastPing = Clock::now();

	while (!stopping) {
		std::deque<std::string> pending;
		{
			std::lock_guard<std::mutex> lock(mutex);
			pending.swap(outbox);
		}
		for (const auto &text : pending) {
			if (!send_all(curl, sock, text.data(), text.size(), CURLWS_TEXT)) {
				return;
			}
		}
		if (Clock::now() - lastPing > pingEvery) {
			if (!send_all(curl, sock, "", 0, CURLWS_PING)) {
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
			message.append(buffer, received);
			// a message can arrive in several reads and in several fragments
			if (meta->bytesleft == 0 && !(meta->flags & CURLWS_CONT)) {
				handle_message(message);
				message.clear();
			}
		}
		wait_socket(sock, false);
	}
}

void run(std::string address)
{
	int wait = 2;
	while (!stopping) {
		state = BotLink::State::Connecting;
		CURL *curl = curl_easy_init();
		if (!curl) {
			break;
		}
		curl_easy_setopt(curl, CURLOPT_URL, address.c_str());
		curl_easy_setopt(curl, CURLOPT_CONNECT_ONLY, 2L); // do the WebSocket handshake, then hand over
		curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
		curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
		curl_easy_setopt(curl, CURLOPT_USERAGENT, "fiji-suite/" PROJECT_VERSION);
		const CURLcode res = curl_easy_perform(curl);
		if (res == CURLE_OK) {
			wait = 2;
			{
				std::lock_guard<std::mutex> lock(mutex);
				outbox.clear();
				outbox.emplace_back("{\"type\":\"hello\",\"plugin\":\"fiji-suite\",\"version\":\"" PROJECT_VERSION "\"}");
				lastError.clear();
			}
			state = BotLink::State::Connected;
			blog(LOG_INFO, "[Fiji Suite] Bot link connected");
			session(curl);
			blog(LOG_INFO, "[Fiji Suite] Bot link disconnected");
			if (!stopping) {
				set_error("connection lost");
			}
		} else {
			set_error(curl_easy_strerror(res));
		}
		curl_easy_cleanup(curl);
		if (stopping) {
			break;
		}
		state = BotLink::State::Connecting;
		// back off between attempts, but notice a stop quickly
		for (int i = 0; i < wait * 10 && !stopping; i++) {
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
		wait = wait * 2 > 60 ? 60 : wait * 2;
	}
	finished = true;
}

} // namespace

void BotLink::Start(const std::string &address)
{
	Stop();
	if (address.empty()) {
		return;
	}
	if (address.rfind("ws://", 0) != 0 && address.rfind("wss://", 0) != 0) {
		state = State::BadAddress;
		return;
	}
	stopping = false;
	finished = false;
	state = State::Connecting;
	worker = std::thread(run, address);
}

void BotLink::Stop()
{
	stopping = true;
	if (worker.joinable()) {
		// A request the bot sent may be waiting on this (the UI) thread, so keep it responsive
		// until the worker has wound down.
		while (!finished) {
			QCoreApplication::processEvents();
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
		}
		worker.join();
	}
	state = State::Off;
	std::lock_guard<std::mutex> lock(mutex);
	outbox.clear();
	lastError.clear();
}

BotLink::State BotLink::GetState()
{
	return state;
}

QString BotLink::LastError()
{
	std::lock_guard<std::mutex> lock(mutex);
	return QString::fromStdString(lastError);
}

void fiji_emit_event(const char *event, obs_data_t *data)
{
	if (vendor) {
		obs_websocket_vendor_emit_event(vendor, event, data);
	}
	if (state == BotLink::State::Connected) {
		OBSDataAutoRelease message = obs_data_create();
		obs_data_set_string(message, "type", "event");
		obs_data_set_string(message, "event", event);
		if (data) {
			obs_data_set_obj(message, "data", data);
		}
		queue(obs_data_get_json(message));
	}
}

bool fiji_events_wanted()
{
	return vendor || state == BotLink::State::Connected;
}
