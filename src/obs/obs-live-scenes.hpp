#pragma once

// libobs implementation of LiveScenesObs: the frontend's scene list and save callback, and the main canvas's
// source_rename signal.

#include "live-scenes-ports.hpp"

struct calldata;
typedef struct calldata calldata_t;
struct obs_data;
typedef struct obs_data obs_data_t;

namespace fiji::obs {

class ObsLiveScenes final : public LiveScenesObs {
public:
	ObsLiveScenes() = default;
	~ObsLiveScenes() override;

	void listen(Listener listener) override;
	QStringList sceneNames() override;
	QString uuidOf(const QString &name) override;
	Program program() override;
	void showScene(const QString &uuid, const QString &name) override;

private:
	static void saveLoad(obs_data_t *data, bool saving, void *param);
	static void sourceRenamed(void *param, calldata_t *cd);

	Listener listener_;
	bool listening_ = false;
};

} // namespace fiji::obs
