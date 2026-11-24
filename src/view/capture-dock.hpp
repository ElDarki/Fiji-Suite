#pragma once

// VIEW: the Capture dock. One row per game or window capture source: a button with the windows it can capture
// (picking one points the source at it), the source's name, and what it captures now. Follows
// controller::CaptureController.

#include "../controller/capture-controller.hpp"

#include <QFrame>
#include <QHash>

class QLabel;
class QToolButton;
class QVBoxLayout;

namespace fiji::view {

class CaptureDock : public QFrame {
	Q_OBJECT

public:
	// configText: the translated label of each row's button.
	CaptureDock(controller::CaptureController &captures, const QString &configText, QWidget *parent = nullptr);

	// For tests: the rows by source uuid.
	struct Row {
		QFrame *frame = nullptr;
		QToolButton *button = nullptr;
		QLabel *name = nullptr;
		QLabel *capture = nullptr;
	};
	const QHash<QString, Row> &rows() const { return rows_; }
	// The menu a row's button shows (built when it is pressed).
	void fillWindowMenu(const QString &uuid, class QMenu &menu);

private:
	void add(const controller::CaptureEntry &entry);
	void remove(const QString &uuid);
	void update(const controller::CaptureEntry &entry);

	controller::CaptureController &captures_;
	QString configText_;
	QVBoxLayout *list_ = nullptr;
	QHash<QString, Row> rows_;
};

} // namespace fiji::view
