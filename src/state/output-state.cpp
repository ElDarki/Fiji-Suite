#include "output-state.hpp"

namespace fiji::state {

QString outputStateName(OutputStateId state)
{
	static const char *const names[] = {"IDLE", "STARTING", "ACTIVE", "RECONNECTING", "STOPPING", "FAILED"};
	return QString::fromLatin1(names[int(state)]);
}

// Defaults shared by the states: OBS reports the output live -> ACTIVE; it stopped -> FAILED with an error, IDLE without.
void OutputState::onStarted(OutputLifecycle &output)
{
	output.transitionTo(std::make_unique<OutputActive>());
}

void OutputState::onStopped(OutputLifecycle &output, const QString &error)
{
	if (error.isEmpty()) {
		output.transitionTo(std::make_unique<OutputIdle>());
	} else {
		output.transitionTo(std::make_unique<OutputFailed>(), error);
	}
}

void OutputIdle::onStartRequested(OutputLifecycle &output)
{
	output.transitionTo(std::make_unique<OutputStarting>());
}

void OutputFailed::onStartRequested(OutputLifecycle &output)
{
	output.transitionTo(std::make_unique<OutputStarting>());
}

void OutputStarting::onStopRequested(OutputLifecycle &output)
{
	output.transitionTo(std::make_unique<OutputStopping>());
}

void OutputActive::onReconnecting(OutputLifecycle &output)
{
	output.transitionTo(std::make_unique<OutputReconnecting>());
}

void OutputActive::onStopRequested(OutputLifecycle &output)
{
	output.transitionTo(std::make_unique<OutputStopping>());
}

void OutputReconnecting::onReconnected(OutputLifecycle &output)
{
	output.transitionTo(std::make_unique<OutputActive>());
}

void OutputReconnecting::onStopRequested(OutputLifecycle &output)
{
	output.transitionTo(std::make_unique<OutputStopping>());
}

void OutputStopping::onStopped(OutputLifecycle &output, const QString &error)
{
	// the streamer asked for it: a problem while stopping still ends in FAILED, so it is reported
	if (error.isEmpty()) {
		output.transitionTo(std::make_unique<OutputIdle>());
	} else {
		output.transitionTo(std::make_unique<OutputFailed>(), error);
	}
}

OutputLifecycle::OutputLifecycle(QObject *parent) : QObject(parent), current_(std::make_unique<OutputIdle>()) {}

OutputLifecycle::~OutputLifecycle() = default;

OutputStateId OutputLifecycle::state() const
{
	return current_->id();
}

void OutputLifecycle::startRequested()
{
	current_->onStartRequested(*this);
}

void OutputLifecycle::started()
{
	current_->onStarted(*this);
}

void OutputLifecycle::reconnecting()
{
	current_->onReconnecting(*this);
}

void OutputLifecycle::reconnected()
{
	current_->onReconnected(*this);
}

void OutputLifecycle::stopRequested()
{
	current_->onStopRequested(*this);
}

void OutputLifecycle::stopped(const QString &error)
{
	current_->onStopped(*this, error);
}

void OutputLifecycle::transitionTo(std::unique_ptr<OutputState> next, const QString &error)
{
	const OutputStateId before = state();
	current_ = std::move(next);
	if (state() == OutputStateId::FAILED) {
		error_ = error;
	} else if (state() == OutputStateId::STARTING) {
		error_.clear();
	}
	if (state() != before) {
		emit changed(state(), before, state() == OutputStateId::FAILED ? error_ : QString());
	}
}

} // namespace fiji::state
