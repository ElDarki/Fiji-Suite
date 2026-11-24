#pragma once

// MODEL: the Live Scenes list, a short list of scenes to switch between, saved with the scene collection as
// "live_scenes": [{"name", "uuid"}]. Knows which entry is on program. Qt Core only. Reads are safe from any thread
// (obs-websocket asks from its own); changes come from the thread the model lives on.

#include <QJsonArray>
#include <QList>
#include <QObject>
#include <QString>
#include <mutex>

namespace fiji::model {

struct LiveScene {
	QString name;
	QString uuid;
	bool operator==(const LiveScene &other) const { return name == other.name && uuid == other.uuid; }
};

class LiveScenesModel : public QObject {
	Q_OBJECT

public:
	explicit LiveScenesModel(QObject *parent = nullptr) : QObject(parent) {}

	QList<LiveScene> scenes() const;
	// The entry on program, or -1.
	int current() const;
	QJsonArray toJson() const;

	// Replaces the list (a scene collection was loaded); nothing is on program until setProgram.
	void load(const QJsonArray &scenes);
	// False when the name is empty.
	bool add(const QString &name, const QString &uuid);
	bool removeAt(int row);
	// Removes the first entry with that name.
	bool removeNamed(const QString &name);
	// Moves an entry up (negative) or down; false at the ends. The program entry stays the program entry.
	bool move(int row, int delta);
	// A scene was renamed: entries with the old name follow.
	void renamed(const QString &previous, const QString &next);
	// The scene OBS shows now: the first entry with its name is on program, else the first with its uuid (which then
	// takes the new name); none when neither matches.
	void setProgram(const QString &name, const QString &uuid);

signals:
	void changed();

private:
	mutable std::mutex mutex_;
	QList<LiveScene> scenes_;
	int current_ = -1;
};

} // namespace fiji::model
