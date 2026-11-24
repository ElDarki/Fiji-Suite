#pragma once

// OBS port: what the Live Scenes list needs from OBS (controller::LiveScenesController). Qt Core only.

#include <QJsonArray>
#include <QString>
#include <QStringList>
#include <functional>

namespace fiji::obs {

class LiveScenesObs {
public:
	struct Program {
		QString name;
		QString uuid;
	};
	struct Listener {
		// A scene on the main canvas was renamed (any thread).
		std::function<void(const QString &previous, const QString &next)> renamed;
		// The scene collection is saved: the list to store with it (OBS's thread).
		std::function<QJsonArray()> save;
		// A scene collection was loaded: the list it stored (OBS's thread).
		std::function<void(const QJsonArray &scenes)> load;
	};

	virtual ~LiveScenesObs() = default;
	virtual void listen(Listener listener) = 0;
	// The main canvas's scenes, in OBS's order.
	virtual QStringList sceneNames() = 0;
	// The uuid of the scene with that name; empty when there is none.
	virtual QString uuidOf(const QString &name) = 0;
	virtual Program program() = 0;
	// Puts the scene on program (by uuid, else by name) unless it already is.
	virtual void showScene(const QString &uuid, const QString &name) = 0;
};

} // namespace fiji::obs
