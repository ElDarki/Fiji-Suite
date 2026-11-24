#include "outputs-controller.hpp"

#include <QMetaObject>

namespace fiji::controller {

OutputsController::OutputsController(obs::Outputs &outputs, std::function<void(const QString &)> log, QObject *parent)
	: QObject(parent),
	  outputs_(outputs),
	  log_(std::move(log))
{
}

bool OutputsController::picked(const obs::OutputEntry &output, Which which)
{
	return which == Which::All || (which == Which::Streams && output.stream) ||
	       (which == Which::Recordings && output.record);
}

void OutputsController::startAll(Which which)
{
	QStringList ids;
	for (const auto &o : outputs_.list()) {
		if (picked(o, which)) {
			ids.append(o.id);
		}
	}
	startQueue(ids);
}

void OutputsController::stopAll(Which which)
{
	queue_.clear();
	for (const auto &o : outputs_.list()) {
		if (picked(o, which) && outputs_.active(o.id)) {
			outputs_.stop(o.id);
		}
	}
}

void OutputsController::startGroup(const QString &group)
{
	QStringList ids;
	for (const auto &o : outputs_.list()) {
		if (!o.main && o.group == group) {
			ids.append(o.id);
		}
	}
	startQueue(ids);
}

void OutputsController::stopGroup(const QString &group)
{
	queue_.clear();
	for (const auto &o : outputs_.list()) {
		if (!o.main && o.group == group) {
			outputs_.stop(o.id);
		}
	}
}

QStringList OutputsController::groups() const
{
	QStringList out;
	for (const auto &o : outputs_.list()) {
		if (!o.group.isEmpty() && !out.contains(o.group)) {
			out.append(o.group);
		}
	}
	return out;
}

std::pair<int, int> OutputsController::groupStatus(const QString &group) const
{
	int live = 0, members = 0;
	for (const auto &o : outputs_.list()) {
		if (!o.main && o.group == group) {
			members++;
			live += outputs_.active(o.id) ? 1 : 0;
		}
	}
	return {live, members};
}

void OutputsController::startQueue(const QStringList &ids)
{
	queue_.assign(ids.begin(), ids.end());
	at_ = 0;
	if (queue_.empty()) {
		return;
	}
	if (log_) {
		log_(QStringLiteral("Starting %1 outputs").arg(queue_.size()));
	}
	startNext();
}

void OutputsController::startNext()
{
	if (queue_.empty()) {
		return;
	}
	if (at_ >= queue_.size()) {
		at_ = 0;
	}
	const size_t first = at_;
	while (true) {
		const QString id = queue_[at_];
		// onStarted may come from an output's own thread: the next start happens on this one
		const bool trying = outputs_.start(id, [this] {
			QMetaObject::invokeMethod(this, [this] { startNext(); }, Qt::QueuedConnection);
		});
		if (trying) {
			queue_.erase(queue_.begin() + qsizetype(at_));
			return;
		}
		at_ = (at_ + 1) % queue_.size();
		if (at_ == first) {
			if (log_) {
				log_(QStringLiteral("Went through all outputs and %1 could not be started").arg(queue_.size()));
			}
			queue_.clear();
			return;
		}
	}
}

} // namespace fiji::controller
