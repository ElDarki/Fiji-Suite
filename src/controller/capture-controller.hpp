#pragma once

// CONTROLLER layer: what the Capture dock's menu does: lists a capture's windows and points it at one.

#include <QList>
#include <QString>

#include "../model/captures.hpp"

namespace fiji::obs {
class CapturePort;
}

namespace fiji::controller {

class CaptureController {
public:
	explicit CaptureController(obs::CapturePort &obs);

	QList<model::WindowChoice> windows(const QString &captureKey) const; // the ones with a label
	void choose(const QString &captureKey, const model::WindowChoice &choice);

private:
	obs::CapturePort &obs_;
};

} // namespace fiji::controller
