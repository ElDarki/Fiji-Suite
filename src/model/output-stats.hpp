#pragma once

// MODEL: the Stats dock's table. Each tick takes a snapshot of OBS's encoders, outputs and canvases (raw counters,
// identified by opaque ids) and keeps one row per video encoder / output: names, sizes, totals, per-second rates, and
// a short history of each rate for the graphs. A Qt item model (Qt Core only); the view draws it.

#include <QAbstractTableModel>
#include <QList>
#include <QString>
#include <functional>

namespace fiji::model {

struct StatsEncoder {
	quintptr id = 0;
	QString name;
	quintptr video = 0; // the video it encodes (matches StatsCanvas::video)
	bool isVideo = true;
	bool active = false;
	quint32 encodedFrames = 0;
	quint32 width = 0, height = 0;
};

struct StatsOutput {
	quintptr id = 0;
	QString name;
	bool active = false;
	bool encoded = true;
	QList<quintptr> videoEncoders;
	quintptr video = 0; // raw (not encoded) outputs
	quint32 activeDelay = 0, droppedFrames = 0;
	quint64 totalBytes = 0;
	quint32 totalFrames = 0;
};

struct StatsCanvas {
	QString name;
	quintptr video = 0;
	quint32 skippedFrames = 0, totalFrames = 0;
	quint32 width = 0, height = 0;
};

struct StatsSnapshot {
	QList<StatsEncoder> encoders;
	QList<StatsOutput> outputs;
	QList<StatsCanvas> canvases;
};

struct StatsRow {
	quintptr output = 0, encoder = 0, video = 0;
	bool hasCanvas = false;
	QString outputName, encoderName, canvasName;
	QVariant canvasColor; // the canvas's colour as Fiji Suite stores it (an integer), or none
	bool updated = false;
	quint32 skippedFrames = 0, canvasFrames = 0, canvasFps = 0, canvasWidth = 0, canvasHeight = 0;
	quint32 encodedFrames = 0, encodedFps = 0, encodedWidth = 0, encodedHeight = 0;
	quint32 activeDelay = 0, droppedFrames = 0;
	quint64 outputBytes = 0;
	quint32 outputBitrate = 0, outputFrames = 0, outputFps = 0; // bitrate in kb/s
	QList<quint32> outputFpsHistory, outputBitrateHistory, encodedFpsHistory, canvasFpsHistory;
};

// What the Stats dock calls OBS's own outputs.
struct BuiltinOutputNames {
	QString record = QStringLiteral("BuiltinRecord");
	QString stream = QStringLiteral("BuiltinStream");
	QString virtualCam = QStringLiteral("BuiltinVirtualCamera");
};
QString displayOutputName(const QString &raw, const BuiltinOutputNames &names);
QString displayEncoderName(const QString &raw);

class OutputStatsTable : public QAbstractTableModel {
	Q_OBJECT

public:
	enum Column {
		OutputName,
		OutputFps,
		OutputFrames,
		OutputBitrate,
		OutputData,
		EncoderName,
		EncoderResolution,
		EncoderFps,
		EncoderFrames,
		CanvasName,
		CanvasResolution,
		CanvasFps,
		CanvasFrames,
		CanvasSkipped,
		ColumnCount
	};
	// Qt::UserRole: a rate column's history (QList<quint32>, oldest first); ColorRole: the row's canvas colour.
	static constexpr int GraphRole = Qt::UserRole;
	static constexpr int ColorRole = Qt::UserRole + 1;
	static constexpr int kHistory = 600;

	using Translate = std::function<QString(const char *key)>;
	using CanvasColor = std::function<QVariant(const QString &canvas)>;
	explicit OutputStatsTable(Translate translate = {}, CanvasColor canvasColor = {}, BuiltinOutputNames names = {},
				  QObject *parent = nullptr);

	// One tick (a second) of OBS's counters.
	void update(const StatsSnapshot &snapshot);
	void clear();
	const QList<StatsRow> &rows() const { return rows_; }
	static bool defaultVisible(int column);

	int rowCount(const QModelIndex &parent = QModelIndex()) const override;
	int columnCount(const QModelIndex &parent = QModelIndex()) const override;
	QVariant data(const QModelIndex &index, int role) const override;
	// "Section\nName" (the header view draws the sections as groups).
	QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

private:
	StatsRow newRow(quintptr output, const QString &outputName, quintptr encoder, const StatsSnapshot &snapshot) const;
	QVariant value(const StatsRow &row, int column) const;

	Translate translate_;
	CanvasColor canvasColor_;
	BuiltinOutputNames names_;
	QList<StatsRow> rows_;
};

} // namespace fiji::model
