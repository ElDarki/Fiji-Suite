#include "command-dispatcher.hpp"

#include <QMetaObject>
#include <QMutexLocker>

namespace fiji::controller {

CommandDispatcher::CommandDispatcher(QObject *parent) : QObject(parent) {}

void CommandDispatcher::registerHandler(CommandHandler *handler)
{
	for (const auto &action : handler->actions()) {
		handlers_.insert(action, handler);
	}
}

void CommandDispatcher::post(Command command, ReplyFn reply)
{
	{
		QMutexLocker lock(&mutex_);
		queue_.emplace_back(std::move(command), std::move(reply));
	}
	// One drain per post; a drain that finds the queue empty (an earlier one took everything) does nothing.
	QMetaObject::invokeMethod(this, [this] { drain(); }, Qt::QueuedConnection);
}

int CommandDispatcher::pending() const
{
	QMutexLocker lock(&mutex_);
	return int(queue_.size());
}

void CommandDispatcher::drain()
{
	for (;;) {
		std::pair<Command, ReplyFn> next;
		{
			QMutexLocker lock(&mutex_);
			if (queue_.empty()) {
				return;
			}
			next = std::move(queue_.front());
			queue_.pop_front();
		}
		CommandResult result;
		if (auto handler = handlers_.value(next.first.action)) {
			result = handler->handle(next.first);
		} else {
			result.ok = false;
			result.code = QStringLiteral("UNKNOWN_ACTION");
			result.message = QStringLiteral("Nothing handles %1").arg(next.first.action);
		}
		if (next.second) {
			next.second(result);
		}
	}
}

} // namespace fiji::controller
