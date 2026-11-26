#pragma once

// MODEL layer: the Stats dock's rows. Once a second it gets OBS's raw counters (StatsSnapshot, from the
// obs::StatsSource port) and keeps one row per video pipeline: an encoder, the output that uses it, and the canvas
// it encodes, with per-second rates worked out from the counters. Qt Core only.
//
// Rows are matched as OBS reports them: an active video encoder gets a row; an output claims the rows of its
// encoders (or gets rows of its own); a canvas claims the rows that encode its video. Rows stay until the dock is
// hidden (clear()); a row nothing updated this second shows rates of zero.

#include <QList>
#include <QString>

namespace fiji::model {

struct EncoderSample {
	quintptr id = 0;
	QString name;       // OBS's name
	quintptr video = 0; // the video it encodes (its parent's, before scaling)
	bool active = false;
	quint32 width = 0, height = 0;
	quint32 frames = 0;
};

struct OutputSample {
	quintptr id = 0;
	QString name;
	bool encoded = false;
	bool active = false;
	QList<EncoderSample> videoEncoders; // encoded outputs
	quintptr video = 0;                 // raw outputs
	quint64 bytes = 0;
	quint32 frames = 0, dropped = 0, delay = 0;
};

struct CanvasSample {
	quintptr id = 0;
	QString name;
	qint64 color = -1; // the colour the user gave the canvas (OBS's packed int), or -1
	quintptr video = 0;
	quint32 frames = 0, skipped = 0;
	quint32 width = 0, height = 0; // 0 when unknown
};

struct StatsSnapshot {
	QList<EncoderSample> encoders; // video encoders
	QList<OutputSample> outputs;
	QList<CanvasSample> canvases;
};

struct StatsRow {
	quintptr output = 0, encoder = 0, canvas = 0, video = 0;
	QString outputName, encoderName, canvasName; // display names
	qint64 canvasColor = -1;
	quint32 skippedFrames = 0, canvasFrames = 0, canvasFps = 0, canvasWidth = 0, canvasHeight = 0;
	quint32 encodedFrames = 0, encodedFps = 0, encodedWidth = 0, encodedHeight = 0;
	quint32 activeDelay = 0, droppedFrames = 0;
	quint64 outputBytes = 0;
	quint32 outputBitrate = 0; // kbit/s
	quint32 outputFrames = 0, outputFps = 0;
	bool updated = false;
};

// What the dock calls OBS's own outputs.
struct StatsNames {
	QString record, stream, virtualCamera;
};
QString outputDisplayName(const QString &obsName, const StatsNames &names);
QString encoderDisplayName(const QString &obsName);

struct StatsUpdate {
	bool reset = false; // all rows went
	int inserted = 0;   // rows added at the end
	int firstChanged = -1, lastChanged = -1;
};

class OutputStats {
public:
	explicit OutputStats(StatsNames names = {});

	const QList<StatsRow> &rows() const { return rows_; }
	void clear();
	StatsUpdate update(const StatsSnapshot &snapshot);

private:
	void changed(int row);
	StatsRow newRow(quintptr output, const QString &outputName, const EncoderSample *encoder, quintptr video);

	StatsNames names_;
	QList<StatsRow> rows_;
	int first_ = -1, last_ = -1;
};

} // namespace fiji::model
