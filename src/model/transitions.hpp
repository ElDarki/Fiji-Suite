#pragma once

// MODEL layer: the rules behind the Transitions dock. Qt Core only.

#include <QList>
#include <QString>

namespace fiji::model {

struct TransitionInfo {
	QString name;
	bool configurable = false; // has properties; only these can be removed, renamed or copied
};

struct TransitionType {
	QString id;
	QString displayName;
};

// Why a name cannot be given to a transition of this list (ignoring the one being renamed).
enum class TransitionNameProblem { None, Empty, Taken };
TransitionNameProblem transitionNameProblem(const QString &name, const QList<TransitionInfo> &list,
					    const QString &renaming = QString());

// The "copy from main" menu: every main transition, offered when it has properties and the canvas has none of
// that name yet.
struct CopyChoice {
	QString name;
	bool enabled = false;
};
QList<CopyChoice> copyChoices(const QList<TransitionInfo> &main, const QList<TransitionInfo> &canvas);

} // namespace fiji::model
