#pragma once

// CONTROLLER layer: the thread-safe dispatcher. Bridge commands arrive on the link thread, hotkeys and docks on the Qt
// thread; all of them post() here. One consumer runs every command, in order, on the OBS/UI thread (queued with
// QMetaObject::invokeMethod(Qt::QueuedConnection)), where the handlers call libobs through ObsApi.
// Handlers are the only code outside src/obs/ that drives libobs.

#include "command.hpp"

#include <QHash>
#include <QMutex>
#include <QObject>
#include <QStringList>
#include <deque>
#include <utility>

namespace fiji::controller {

class CommandHandler {
public:
	virtual ~CommandHandler() = default;
	// The actions this handler serves, e.g. {"scene.switch", "transition.set"}.
	virtual QStringList actions() const = 0;
	// Runs on the consumer thread. Never throws: failures come back as CommandResult{ok = false}.
	virtual CommandResult handle(const Command &command) = 0;
};

class CommandDispatcher : public QObject {
	Q_OBJECT

public:
	explicit CommandDispatcher(QObject *parent = nullptr);

	// Registration happens on start-up, on the UI thread, before any post().
	void registerHandler(CommandHandler *handler);

	// Thread-safe; callable from any thread. Unknown actions reply UNKNOWN_ACTION.
	void post(Command command, ReplyFn reply = {});

	// Runs a command now and returns its result, for callers that must answer at once (obs-websocket requests,
	// the request table). On the dispatcher's thread it runs directly; from another thread it waits for that thread
	// (so never call it from a thread the UI thread is waiting on).
	CommandResult call(Command command);

	// Pending commands (for the stats dock and tests).
	int pending() const;

private:
	void drain(); // consumer, on this object's thread
	CommandResult run(const Command &command);

	mutable QMutex mutex_;
	std::deque<std::pair<Command, ReplyFn>> queue_;
	QHash<QString, CommandHandler *> handlers_;
};

} // namespace fiji::controller
