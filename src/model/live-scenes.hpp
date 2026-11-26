#pragma once

// MODEL layer: the Live Scenes list, the short list of scenes a streamer switches between during a show. Each entry
// keeps the scene's name and OBS uuid, so a renamed scene is still found. It is saved with the scene collection
// (src/obs/obs-live-scenes.cpp). Qt Core only.
//
// Changes happen on the UI thread; entries() and toJson() may be read from any thread (obs-websocket requests).

#include <QJsonArray>
#include <QList>
#include <QMutex>
#include <QObject>
#include <QString>

namespace fiji::model {

struct LiveScene {
	QString name;
	QString uuid;
	bool operator==(const LiveScene &other) const { return name == other.name && uuid == other.uuid; }
};

class LiveScenes : public QObject {
	Q_OBJECT

public:
	explicit LiveScenes(QObject *parent = nullptr);

	QList<LiveScene> entries() const;
	int count() const;
	int live() const; // the entry showing on program, or -1

	void replaceAll(const QList<LiveScene> &entries);
	void add(const LiveScene &entry);
	bool removeAt(int index);
	bool removeNamed(const QString &name); // the first entry with that name
	// Moves an entry by delta places; returns its new index, or -1 when it would leave the list.
	int move(int index, int delta);
	// A scene was renamed in OBS: every entry with the old name follows.
	void renamed(const QString &previous, const QString &name);
	// Program now shows this scene: marks the entry with its name, or else the one with its uuid (which then takes
	// the new name). Returns the entry's index, or -1 when the scene is not in the list.
	int markLive(const QString &name, const QString &uuid);

	QJsonArray toJson() const; // [{"name", "uuid"}]: the scene collection's "live_scenes"
	static QList<LiveScene> fromJson(const QJsonArray &array);

signals:
	void changed();             // entries added, removed, moved or renamed
	void liveChanged(int index); // -1: none

private:
	void setLive(int index);

	mutable QMutex mutex_;
	QList<LiveScene> entries_;
	int live_ = -1;
};

} // namespace fiji::model
