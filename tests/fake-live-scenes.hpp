#pragma once

// In-memory LiveScenesObs for the tests: a list of scenes, a program scene, and the listener for the test to call.

#include "../src/obs/live-scenes-ports.hpp"

#include <QHash>

struct FakeLiveScenes final : fiji::obs::LiveScenesObs {
	Listener listener;
	QStringList names{"Intro", "Game", "Outro"};
	QHash<QString, QString> uuids{{"Intro", "u-intro"}, {"Game", "u-game"}, {"Outro", "u-outro"}};
	Program now;
	QStringList shown;

	void listen(Listener l) override { listener = std::move(l); }
	QStringList sceneNames() override { return names; }
	QString uuidOf(const QString &name) override { return uuids.value(name); }
	Program program() override { return now; }
	void showScene(const QString &uuid, const QString &name) override
	{
		shown << uuid;
		now = {name, uuid};
	}
};
