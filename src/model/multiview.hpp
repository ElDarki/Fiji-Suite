#pragma once

// MODEL layer: what the Multiview shows about each canvas, apart from its picture: which outputs it feeds, each
// output's status line (idle, live for how long, reconnecting, bitrate, dropped frames) and whether the canvas is on
// air. Qt Core only. The tiles that draw the canvases are still in src/docks/multiview-dock.cpp.

#include <QList>
#include <QMap>
#include <QString>

namespace fiji::model {

struct MultiviewOutput {
	QString name, type, server, canvas;
	bool main = false; // one of OBS's own outputs
	bool active = false;
	bool reconnecting = false;
	qint64 liveSeconds = 0;
	quint64 totalBytes = 0;
	qint64 totalFrames = 0, droppedFrames = 0;
};

// Outputs with no canvas, and OBS's own outputs, belong to the main canvas.
bool outputOnTile(const MultiviewOutput &output, const QString &canvasName, bool mainTile);

// As square a grid as the number of tiles allows.
int multiviewColumns(int tiles);

// "mm:ss", or "hh:mm:ss" from an hour on (wrapping at a day).
QString liveTime(qint64 seconds);

// An output is flagged once it drops this share of its frames.
constexpr double droppedWarningPercent = 1.0;

enum class ChipState { Idle, Live, Reconnecting };

struct Chip {
	QString name, type, server;
	ChipState state = ChipState::Idle;
	qint64 liveSeconds = 0;
	bool showRate = false; // streams and FFmpeg outputs
	int kbps = 0;
	double droppedPercent = -1; // -1: none dropped (or nothing sent yet)
	bool warning = false;
};

// One tile's outputs, with the bitrate worked out between updates.
class TileOutputs {
public:
	TileOutputs(QString canvasName, bool mainTile);

	struct Update {
		QList<Chip> chips;    // this tile's outputs, in the order given
		QStringList gone;     // outputs no longer shown
		bool live = false;    // any of them live: the on-air light
	};
	Update update(const QList<MultiviewOutput> &outputs, qint64 nowMs);

private:
	struct Rate {
		quint64 bytes = 0;
		qint64 time = 0;
		int kbps = 0;
	};
	QString canvasName_;
	bool mainTile_;
	QMap<QString, Rate> rates_;
};

} // namespace fiji::model
