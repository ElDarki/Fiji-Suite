#pragma once

// BRIDGE: the WebSocket to the bot (libcurl, which ships with OBS). The plugin dials out (ws:// or wss://), so nothing
// on the streaming PC has to be reachable. It only moves text frames; BridgeAcl gives them meaning.
// One worker thread per started link: it connects, runs the session, and reconnects with a growing pause.

#include <QByteArray>
#include <QString>
#include <atomic>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

namespace fiji::bridge {

class LinkTransport {
public:
	// All called on the worker thread.
	struct Callbacks {
		std::function<void()> opened;
		std::function<void(const QByteArray &frame)> frame;
		std::function<void()> closed;                         // an open connection ended
		std::function<void(const QString &error)> attemptFailed; // a connection attempt failed
		std::function<void(const QString &line)> log;
	};

	LinkTransport(Callbacks callbacks, std::string userAgent);
	~LinkTransport();

	static bool validAddress(const QString &address); // ws:// or wss://

	void start(const std::string &address); // restarts when running
	void stop();                            // closes and waits for the worker
	void stopRetrying();                    // ends after the current connection (the bot refused this plugin)
	void send(const QByteArray &frame);     // thread-safe; dropped unless connected

private:
	void run(std::string address);
	void session(void *curl);

	Callbacks callbacks_;
	std::string userAgent_;
	std::thread worker_;
	std::atomic<bool> stopping_{false};
	std::atomic<bool> retry_{true};
	std::atomic<bool> open_{false};
	std::mutex mutex_; // guards outbox_
	std::deque<QByteArray> outbox_;
};

} // namespace fiji::bridge
