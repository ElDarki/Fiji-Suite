#pragma once

// CONTROLLER: the Live Scenes list. Changes model::LiveScenesModel (from the dock and obs-websocket), puts a picked
// entry on program, follows OBS's program scene, scene renames and the scene collection's save and load. Qt Core only.

#include "../model/live-scenes-model.hpp"
#include "../obs/live-scenes-ports.hpp"

#include <QObject>

namespace fiji::controller {

class LiveScenesController : public QObject {
	Q_OBJECT

public:
	LiveScenesController(model::LiveScenesModel &model, obs::LiveScenesObs &obs, QObject *parent = nullptr);

	// Starts listening to OBS (once).
	void start();
	model::LiveScenesModel &model() { return model_; }

	// The scenes that can be added.
	QStringList sceneNames() { return obs_.sceneNames(); }
	// Adds the scene with that name; false when OBS has none.
	bool addScene(const QString &name);
	bool removeAt(int row) { return model_.removeAt(row); }
	bool removeNamed(const QString &name) { return model_.removeNamed(name); }
	bool move(int row, int delta) { return model_.move(row, delta); }
	// Puts the entry on program.
	void pick(int row);
	// OBS's program scene changed (or may have).
	void programChanged();

private:
	model::LiveScenesModel &model_;
	obs::LiveScenesObs &obs_;
	bool started_ = false;
};

} // namespace fiji::controller
