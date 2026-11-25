#include "output-states.hpp"

namespace fiji::state {

using S = OutputStateId;
using I = OutputInput;

OutputStateId nextOutputState(OutputStateId from, OutputInput input)
{
	if (input == I::Failed) {
		return from == S::FAILED ? from : S::FAILED;
	}
	switch (from) {
	case S::IDLE:
		return input == I::StartAsked ? S::STARTING : input == I::Started ? S::ACTIVE : from;
	case S::STARTING:
		return input == I::Started        ? S::ACTIVE
		       : input == I::Reconnecting ? S::RECONNECTING
		       : input == I::StopAsked    ? S::STOPPING
		       : input == I::Stopped      ? S::IDLE
						  : from;
	case S::ACTIVE:
		return input == I::Reconnecting ? S::RECONNECTING : input == I::StopAsked ? S::STOPPING : input == I::Stopped ? S::IDLE : from;
	case S::RECONNECTING:
		return (input == I::Started || input == I::Reconnected) ? S::ACTIVE
		       : input == I::StopAsked				? S::STOPPING
		       : input == I::Stopped				? S::IDLE
									: from;
	case S::STOPPING:
		return input == I::Stopped ? S::IDLE : from;
	case S::FAILED:
		return input == I::StartAsked ? S::STARTING : input == I::Started ? S::ACTIVE : input == I::Stopped ? S::IDLE : from;
	}
	return from;
}

QString outputStateName(OutputStateId state)
{
	switch (state) {
	case S::IDLE:
		return QStringLiteral("IDLE");
	case S::STARTING:
		return QStringLiteral("STARTING");
	case S::ACTIVE:
		return QStringLiteral("ACTIVE");
	case S::RECONNECTING:
		return QStringLiteral("RECONNECTING");
	case S::STOPPING:
		return QStringLiteral("STOPPING");
	case S::FAILED:
		return QStringLiteral("FAILED");
	}
	return QStringLiteral("IDLE");
}

OutputStates::OutputStates(QObject *parent) : QObject(parent) {}

OutputStateId OutputStates::state(const QString &output) const
{
	return states_.value(output, S::IDLE);
}

QString OutputStates::lastError(const QString &output) const
{
	return errors_.value(output);
}

void OutputStates::input(const QString &output, OutputInput input, const QString &error)
{
	const S before = state(output);
	const S after = nextOutputState(before, input);
	if (after == before) {
		return;
	}
	if (after == S::IDLE) {
		states_.remove(output);
	} else {
		states_.insert(output, after);
	}
	if (after == S::FAILED) {
		errors_.insert(output, error);
	} else if (after == S::STARTING || after == S::ACTIVE) {
		errors_.remove(output);
	}
	emit stateChanged(output, after, after == S::FAILED ? error : QString());
}

void OutputStates::onPluginEvent(const QString &event, const QString &output, const QString &error)
{
	if (event == QStringLiteral("start_output")) {
		input(output, I::Started);
	} else if (event == QStringLiteral("output_reconnecting")) {
		input(output, I::Reconnecting);
	} else if (event == QStringLiteral("output_reconnected")) {
		input(output, I::Reconnected);
	} else if (event == QStringLiteral("stop_output")) {
		input(output, error.isEmpty() ? I::Stopped : I::Failed, error);
	}
}

} // namespace fiji::state
