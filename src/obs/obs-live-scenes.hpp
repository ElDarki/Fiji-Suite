#pragma once

// OBS adapters for Live Scenes:
//   ObsScenes          the ScenePort on OBS's main canvas (frontend API).
//   ObsLiveScenesSync  saves the list with the scene collection ("live_scenes", as before) and follows scene
//                      renames on the main canvas. Built once OBS's frontend is up; undone when destroyed.

#include "scene-port.hpp"

struct obs_data;
struct calldata;

namespace fiji::model {
class LiveScenes;
}

namespace fiji::obs {

class ObsScenes final : public ScenePort {
public:
	QStringList sceneNames() override;
	QString uuidOf(const QString &name) override;
	SceneRef programScene() override;
	bool showOnProgram(const SceneRef &scene) override;
};

class ObsLiveScenesSync {
public:
	explicit ObsLiveScenesSync(model::LiveScenes &scenes);
	~ObsLiveScenesSync();
	ObsLiveScenesSync(const ObsLiveScenesSync &) = delete;
	ObsLiveScenesSync &operator=(const ObsLiveScenesSync &) = delete;

private:
	static void save_load(obs_data *save_data, bool saving, void *data);
	static void source_rename(void *data, calldata *cd);
	void followRenames(bool follow);

	model::LiveScenes &scenes_;
};

} // namespace fiji::obs
