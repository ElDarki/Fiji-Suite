#pragma once

// MODEL layer: what "start" or "stop" a target means. A target is "all", a group name, one of OBS's own outputs
// (stream, record, backtrack, virtual_cam, or such an output's name), or a plugin output's name. Qt Core only.

#include "obs-status.hpp"

namespace fiji::model {

struct OutputPlan {
	enum class Kind { None, All, Group, Main, Named };
	Kind kind = Kind::None;
	QString target;     // the group or output name as given
	QString mainKind;   // for Main: stream, record, backtrack or virtual_cam
	QStringList names;  // the outputs it affects
};

OutputPlan planOutputs(const QString &target, const QList<OutputStatus> &outputs);

} // namespace fiji::model
