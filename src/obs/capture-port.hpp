#pragma once

// The OBS port for the Capture dock: the windows a capture source can hook, and choosing one. Free of obs.h; the
// OBS implementation is ObsCaptures (obs-captures.hpp).

#include "../model/captures.hpp"

#include <QList>
#include <QString>
#include <QVariant>

namespace fiji::obs {

using model::WindowChoice;

class CapturePort {
public:
	virtual ~CapturePort() = default;
	virtual QList<WindowChoice> windows(const QString &captureKey) = 0;
	virtual void chooseWindow(const QString &captureKey, const QVariant &value) = 0;
};

} // namespace fiji::obs
