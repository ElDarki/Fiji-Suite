#pragma once

// VIEW: the Live Scenes dock. Shows model::LiveScenesModel with the program entry selected; picking an entry puts it
// on program; the toolbar adds (a menu of OBS's scenes), removes and moves entries. All through
// controller::LiveScenesController.

#include "../controller/live-scenes-controller.hpp"

#include <QFrame>

class QListWidget;
class QMenu;

namespace fiji::view {

class LiveScenesDock : public QFrame {
	Q_OBJECT

public:
	// The toolbar's translated labels.
	struct Texts {
		QString add, remove, moveUp, moveDown;
	};
	LiveScenesDock(controller::LiveScenesController &scenes, const Texts &texts, QWidget *parent = nullptr);

	// For tests.
	QListWidget *list() const { return list_; }
	void fillAddMenu(QMenu &menu);
	void removeCurrent();
	void moveCurrent(int delta);

private:
	void refresh();

	controller::LiveScenesController &scenes_;
	QListWidget *list_ = nullptr;
};

} // namespace fiji::view
