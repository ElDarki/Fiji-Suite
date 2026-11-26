#pragma once

// The OBS port for the main canvas's scenes, as the Live Scenes controller needs them. Free of obs.h; the OBS
// implementation is ObsScenes (obs-live-scenes.hpp).

#include <QString>
#include <QStringList>

namespace fiji::obs {

struct SceneRef {
	QString name;
	QString uuid;
};

class ScenePort {
public:
	virtual ~ScenePort() = default;

	virtual QStringList sceneNames() = 0;              // OBS's scenes, in OBS's order
	virtual QString uuidOf(const QString &name) = 0;   // empty when there is no such scene
	virtual SceneRef programScene() = 0;               // empty when there is none
	// Puts a scene on program (found by uuid, else by name). False when it does not exist.
	virtual bool showOnProgram(const SceneRef &scene) = 0;
};

} // namespace fiji::obs
