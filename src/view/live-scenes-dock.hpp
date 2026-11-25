#pragma once

// VIEW layer: the Live Scenes dock. Shows the model's list with the scene on program selected; clicking an entry
// puts it on program, and the toolbar adds, removes and reorders entries, all through the controller. Texts come
// from the caller (OBS's locale).

#include <QFrame>
#include <QString>

class QListWidget;

namespace fiji::model {
class LiveScenes;
}
namespace fiji::controller {
class LiveScenesController;
}

namespace fiji::view {

struct LiveScenesTexts {
	QString add, remove, moveUp, moveDown;
};

class LiveScenesDock : public QFrame {
	Q_OBJECT

public:
	LiveScenesDock(model::LiveScenes &scenes, controller::LiveScenesController &controller,
		       const LiveScenesTexts &texts, QWidget *parent = nullptr);

private:
	void rebuild();
	void showLive(int index);
	void move(int delta);

	model::LiveScenes &scenes_;
	controller::LiveScenesController &controller_;
	QListWidget *list_ = nullptr;
};

} // namespace fiji::view
