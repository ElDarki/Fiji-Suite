#pragma once

// CONTROLLER layer: what the Live Scenes dock and the obs-websocket requests do with the list. It checks scenes
// against OBS through the ScenePort and keeps the model's live entry in step with program.

#include <QStringList>

namespace fiji::model {
class LiveScenes;
}
namespace fiji::obs {
class ScenePort;
}

namespace fiji::controller {

class LiveScenesController {
public:
	LiveScenesController(model::LiveScenes &scenes, obs::ScenePort &obs);

	QStringList addable() const;           // the scenes the add menu offers
	bool add(const QString &name);         // false when OBS has no such scene
	bool remove(const QString &name);      // the first entry with that name
	bool removeAt(int index);
	int move(int index, int delta);        // the entry's new index, or -1
	bool show(int index);                  // puts the entry's scene on program
	void programChanged();                 // OBS switched scenes: mark the live entry

private:
	model::LiveScenes &scenes_;
	obs::ScenePort &obs_;
};

} // namespace fiji::controller
