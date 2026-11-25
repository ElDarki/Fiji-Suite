#include "output-targets.hpp"

namespace fiji::model {

namespace {
const QStringList kMainOutputs = {"stream", "record", "backtrack", "virtual_cam"};
}

OutputPlan planOutputs(const QString &target, const QList<OutputStatus> &outputs)
{
	OutputPlan plan;
	plan.target = target;
	if (target == QStringLiteral("all")) {
		plan.kind = OutputPlan::Kind::All;
		for (const auto &o : outputs) {
			plan.names.append(o.name);
		}
		return plan;
	}
	for (const auto &o : outputs) {
		if (!o.group.isEmpty() && o.group == target) {
			plan.names.append(o.name);
		}
	}
	if (!plan.names.isEmpty()) {
		plan.kind = OutputPlan::Kind::Group;
		return plan;
	}
	if (kMainOutputs.contains(target)) {
		plan.kind = OutputPlan::Kind::Main;
		plan.mainKind = target;
		for (const auto &o : outputs) {
			if (o.main && o.kind == target) {
				plan.names.append(o.name);
			}
		}
		if (plan.names.isEmpty()) {
			plan.names.append(target);
		}
		return plan;
	}
	for (const auto &o : outputs) {
		if (o.name == target) {
			plan.kind = o.main ? OutputPlan::Kind::Main : OutputPlan::Kind::Named;
			plan.mainKind = o.main ? o.kind : QString();
			plan.names = {target};
			return plan;
		}
	}
	return plan; // None: no such output or group
}

} // namespace fiji::model
