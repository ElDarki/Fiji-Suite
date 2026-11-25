#include "output-controller.hpp"

#include "../model/output-targets.hpp"
#include "../obs/output-port.hpp"
#include "../state/output-states.hpp"

#include <QJsonArray>

namespace fiji::controller {

CommandResult OutputController::handle(const Command &command)
{
	CommandResult result;
	const bool start = command.action == QStringLiteral("output.start");
	const QString target = command.payload.value(QStringLiteral("target")).toString();
	if (target.isEmpty()) {
		result.ok = false;
		result.code = QStringLiteral("BAD_REQUEST");
		result.message = QStringLiteral("Which output?");
		return result;
	}
	const auto list = backend_.outputs();
	const auto plan = model::planOutputs(target, list);
	using Kind = model::OutputPlan::Kind;
	obs::ObsCall call;
	switch (plan.kind) {
	case Kind::None:
		result.ok = false;
		result.code = QStringLiteral("NOT_FOUND");
		result.message = QStringLiteral("There is no output or group called %1").arg(target);
		return result;
	case Kind::All:
		call = start ? backend_.startAll() : backend_.stopAll();
		break;
	case Kind::Group:
		call = start ? backend_.startGroup(target) : backend_.stopGroup(target);
		break;
	case Kind::Main:
		call = start ? backend_.startMain(plan.mainKind) : backend_.stopMain(plan.mainKind);
		break;
	case Kind::Named:
		call = start ? backend_.startOutput(target) : backend_.stopOutput(target);
		break;
	}
	if (!call.ok) {
		result.ok = false;
		result.code = call.code.isEmpty() ? QStringLiteral("OBS_ERROR") : call.code;
		result.message = call.message;
		return result;
	}
	// Only outputs that will really change are marked: a live one asked to start (or a stopped one asked to stop)
	// gets no event from OBS to move it on again.
	for (const auto &o : list) {
		if (plan.names.contains(o.name) && o.active != start) {
			states_.input(o.name, start ? state::OutputInput::StartAsked : state::OutputInput::StopAsked);
		}
	}
	result.result.insert(start ? QStringLiteral("started") : QStringLiteral("stopped"), QJsonArray::fromStringList(plan.names));
	return result;
}

} // namespace fiji::controller
