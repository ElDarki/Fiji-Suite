#pragma once

// MODEL layer: what OBS looks like right now, in the plugin's own words (not the bridge's). Filled by obs::ObsApi,
// read by controllers; the bridge ACL translates it into a SYNC_STATE. Qt Core only.

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

namespace fiji::model {

struct CanvasState {
	QString name;
	bool main = false; // OBS's own main canvas
	QString program;
	QString preview;
	bool studioMode = false;
	QStringList scenes;
	QStringList transitions;
};

struct OutputStatus {
	QString name;
	QString kind; // stream, record, backtrack, virtual_cam, or the output type of a plugin output
	QString canvas;
	QString group;
	bool main = false; // one of OBS's own outputs
	bool active = false;
	bool reconnecting = false;
	QString state; // IDLE, STARTING, ACTIVE, RECONNECTING, STOPPING or FAILED when known; else from active/reconnecting
	qint64 uptime = 0; // seconds
	qint64 droppedFrames = 0;
	qint64 totalFrames = 0;
};

struct DeckSummary {
	QString id;
	QString name;
	int buttons = 0;
};

struct ObsSnapshot {
	QList<CanvasState> canvases;
	QList<OutputStatus> outputs;
	QList<DeckSummary> decks;
	QStringList running; // deck buttons running now

	// Carried inside a controller::CommandResult (model words, not the wire format).
	QJsonObject toJson() const;
	static ObsSnapshot fromJson(const QJsonObject &json);
};

} // namespace fiji::model
