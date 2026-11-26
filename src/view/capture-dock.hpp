#pragma once

// VIEW layer: the Capture dock. One row per game or window capture: a settings button whose menu points the capture
// at a window (through the controller), the source's name, and the window it has hooked. Texts come from the caller.

#include <QFrame>
#include <QString>

class QVBoxLayout;

namespace fiji::model {
class Captures;
}
namespace fiji::controller {
class CaptureController;
}

namespace fiji::view {

class CaptureDock : public QFrame {
	Q_OBJECT

public:
	CaptureDock(model::Captures &captures, controller::CaptureController &controller, const QString &configText,
		    QWidget *parent = nullptr);

private:
	QWidget *makeRow(int index);
	void refresh(int index);

	model::Captures &captures_;
	controller::CaptureController &controller_;
	QString configText_;
	QVBoxLayout *rows_ = nullptr;
};

} // namespace fiji::view
