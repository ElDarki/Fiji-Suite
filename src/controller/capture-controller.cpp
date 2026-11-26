#include "capture-controller.hpp"

#include "../obs/capture-port.hpp"

namespace fiji::controller {

CaptureController::CaptureController(obs::CapturePort &obs) : obs_(obs) {}

QList<model::WindowChoice> CaptureController::windows(const QString &captureKey) const
{
	QList<model::WindowChoice> shown;
	for (const auto &choice : obs_.windows(captureKey)) {
		if (!choice.label.isEmpty()) {
			shown.append(choice);
		}
	}
	return shown;
}

void CaptureController::choose(const QString &captureKey, const model::WindowChoice &choice)
{
	if (!captureKey.isEmpty() && choice.value.isValid()) {
		obs_.chooseWindow(captureKey, choice.value);
	}
}

} // namespace fiji::controller
