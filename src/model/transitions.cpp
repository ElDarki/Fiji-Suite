#include "transitions.hpp"

namespace fiji::model {

static bool hasName(const QList<TransitionInfo> &list, const QString &name)
{
	for (const auto &t : list) {
		if (t.name == name) {
			return true;
		}
	}
	return false;
}

TransitionNameProblem transitionNameProblem(const QString &name, const QList<TransitionInfo> &list,
					    const QString &renaming)
{
	if (name.isEmpty()) {
		return TransitionNameProblem::Empty;
	}
	if (name != renaming && hasName(list, name)) {
		return TransitionNameProblem::Taken;
	}
	return TransitionNameProblem::None;
}

QList<CopyChoice> copyChoices(const QList<TransitionInfo> &main, const QList<TransitionInfo> &canvas)
{
	QList<CopyChoice> choices;
	for (const auto &t : main) {
		choices.append({t.name, t.configurable && !hasName(canvas, t.name)});
	}
	return choices;
}

} // namespace fiji::model
