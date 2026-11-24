#include "output-stats.hpp"

namespace fiji::model {

namespace {
quint32 rate(quint64 now, quint64 before)
{
	return now > before ? quint32(now - before) : 0;
}

void remember(QList<quint32> &history, quint32 value)
{
	history.append(value);
	if (history.size() > OutputStatsTable::kHistory) {
		history.removeFirst();
	}
}

const StatsEncoder *encoderIn(const StatsSnapshot &snapshot, quintptr id)
{
	for (const auto &encoder : snapshot.encoders) {
		if (encoder.id == id) {
			return &encoder;
		}
	}
	return nullptr;
}

struct ColumnInfo {
	const char *section;
	const char *name;
	bool visible;
	int alignment;
};
const ColumnInfo kColumns[OutputStatsTable::ColumnCount] = {
	{"Output", "Name", true, Qt::AlignLeft | Qt::AlignVCenter},
	{"Output", "FPS", true, Qt::AlignRight | Qt::AlignVCenter},
	{"Output", "TotalFrames", true, Qt::AlignRight | Qt::AlignVCenter},
	{"Output", "Bitrate", true, Qt::AlignRight | Qt::AlignVCenter},
	{"Output", "TotalData", true, Qt::AlignRight | Qt::AlignVCenter},
	{"Encoder", "Name", false, Qt::AlignLeft | Qt::AlignVCenter},
	{"Encoder", "Resolution", false, Qt::AlignCenter},
	{"Encoder", "FPS", true, Qt::AlignRight | Qt::AlignVCenter},
	{"Encoder", "TotalFrames", true, Qt::AlignRight | Qt::AlignVCenter},
	{"Canvas", "Name", false, Qt::AlignLeft | Qt::AlignVCenter},
	{"Canvas", "Resolution", false, Qt::AlignCenter},
	{"Canvas", "FPS", true, Qt::AlignRight | Qt::AlignVCenter},
	{"Canvas", "TotalFrames", true, Qt::AlignRight | Qt::AlignVCenter},
	{"Canvas", "SkippedFrames", true, Qt::AlignRight | Qt::AlignVCenter},
};

QString size(quint32 width, quint32 height)
{
	return QString::number(width) + QStringLiteral("x") + QString::number(height);
}
} // namespace

QString displayOutputName(const QString &raw, const BuiltinOutputNames &names)
{
	QString name = raw;
	if (name.startsWith(QStringLiteral("Fiji Suite Output "))) {
		return name.mid(18);
	}
	if (name.startsWith(QStringLiteral("fiji_multi_output_"))) {
		return name.mid(18);
	}
	if (name == "simple_file_output" || name == "adv_file_output" || name == "simple_ffmpeg_output" ||
	    name == "adv_ffmpeg_output") {
		return names.record;
	}
	if (name == "simple_stream" || name == "adv_stream" || name == "rtmp multitrack video") {
		return names.stream;
	}
	if (name == "virtualcam_output") {
		return names.virtualCam;
	}
	return name.replace(QLatin1Char('_'), QLatin1Char(' '));
}

QString displayEncoderName(const QString &raw)
{
	QString name = raw;
	if (name.startsWith(QStringLiteral("Fiji Suite Video ")) || name.startsWith(QStringLiteral("Fiji Suite Audio "))) {
		return name.mid(17);
	}
	return name.replace(QLatin1Char('_'), QLatin1Char(' '));
}

OutputStatsTable::OutputStatsTable(Translate translate, CanvasColor canvasColor, BuiltinOutputNames names, QObject *parent)
	: QAbstractTableModel(parent),
	  translate_(std::move(translate)),
	  canvasColor_(std::move(canvasColor)),
	  names_(std::move(names))
{
}

bool OutputStatsTable::defaultVisible(int column)
{
	return column >= 0 && column < ColumnCount && kColumns[column].visible;
}

StatsRow OutputStatsTable::newRow(quintptr output, const QString &outputName, quintptr encoder,
				  const StatsSnapshot &snapshot) const
{
	StatsRow row;
	row.output = output;
	row.outputName = output ? displayOutputName(outputName, names_) : QString();
	row.encoder = encoder;
	row.updated = true;
	if (const auto found = encoder ? encoderIn(snapshot, encoder) : nullptr) {
		row.encoderName = displayEncoderName(found->name);
		row.video = found->video;
		row.encodedWidth = found->width;
		row.encodedHeight = found->height;
		row.encodedFrames = found->encodedFrames; // the first rate is the next second's, not the whole total
	}
	return row;
}

void OutputStatsTable::update(const StatsSnapshot &snapshot)
{
	for (auto &row : rows_) {
		row.updated = false;
	}
	QList<StatsRow> added;
	auto addRow = [&added](StatsRow row) { added.append(std::move(row)); };
	// new rows go in as one insertion (the view is told before they exist)
	auto insertAdded = [this, &added] {
		if (added.isEmpty()) {
			return;
		}
		const int first = int(rows_.size());
		beginInsertRows(QModelIndex(), first, first + int(added.size()) - 1);
		rows_.append(added);
		endInsertRows();
		added.clear();
	};

	// video encoders: their frames; a new active one gets a row
	for (const auto &encoder : snapshot.encoders) {
		bool found = false;
		for (auto &row : rows_) {
			if (row.encoder == encoder.id) {
				found = true;
				if (encoder.isVideo) {
					row.encodedFps = rate(encoder.encodedFrames, row.encodedFrames);
					row.encodedFrames = encoder.encodedFrames;
					row.updated = true;
				}
			}
		}
		if (!found && encoder.active && encoder.isVideo) {
			bool pending = false;
			for (const auto &row : added) {
				pending = pending || row.encoder == encoder.id;
			}
			if (!pending) {
				addRow(newRow(0, QString(), encoder.id, snapshot));
			}
		}
	}
	insertAdded();

	// outputs: an encoder row gets the active output that uses it; an output's own counters; a new active one gets rows
	for (const auto &output : snapshot.outputs) {
		bool found = false;
		for (auto &row : rows_) {
			if (output.encoded && !row.output && row.encoder && output.active &&
			    output.videoEncoders.contains(row.encoder)) {
				row.output = output.id;
				row.outputName = displayOutputName(output.name, names_);
			}
			if (row.output == output.id) {
				row.activeDelay = output.activeDelay;
				row.droppedFrames = output.droppedFrames;
				row.outputBitrate = output.totalBytes > row.outputBytes
							    ? quint32((output.totalBytes - row.outputBytes) * 8 / 1000)
							    : 0;
				row.outputBytes = output.totalBytes;
				row.outputFps = rate(output.totalFrames, row.outputFrames);
				row.outputFrames = output.totalFrames;
				row.updated = true;
				found = true;
			}
		}
		if (found || !output.active) {
			continue;
		}
		// a new row starts from the output's totals, so its first rates are the next second's
		auto fromOutput = [&output](StatsRow row) {
			row.activeDelay = output.activeDelay;
			row.droppedFrames = output.droppedFrames;
			row.outputBytes = output.totalBytes;
			row.outputFrames = output.totalFrames;
			return row;
		};
		if (output.encoded) {
			for (const auto encoder : output.videoEncoders) {
				addRow(fromOutput(newRow(output.id, output.name, encoder, snapshot)));
			}
		} else {
			auto row = fromOutput(newRow(output.id, output.name, 0, snapshot));
			row.video = output.video;
			addRow(row);
		}
	}
	insertAdded();
	if (rows_.isEmpty()) {
		return;
	}

	// canvases: the canvas each row's video belongs to, and its frames
	for (const auto &canvas : snapshot.canvases) {
		for (auto &row : rows_) {
			if (!row.hasCanvas && row.video && row.video == canvas.video) {
				row.hasCanvas = true;
				row.canvasName = canvas.name;
				row.canvasColor = canvasColor_ ? canvasColor_(canvas.name) : QVariant();
				row.canvasFrames = canvas.totalFrames; // its rate starts with the next second
			}
			if ((row.hasCanvas && row.canvasName == canvas.name) || (row.video && row.video == canvas.video)) {
				row.skippedFrames = canvas.skippedFrames;
				row.canvasFps = rate(canvas.totalFrames, row.canvasFrames);
				row.canvasFrames = canvas.totalFrames;
				row.canvasWidth = canvas.width;
				row.canvasHeight = canvas.height;
			}
		}
	}

	// rows OBS no longer reports keep their totals; their rates drop to 0
	for (auto &row : rows_) {
		if (!row.updated) {
			row.encodedFps = row.outputBitrate = row.outputFps = row.canvasFps = 0;
		}
		remember(row.outputFpsHistory, row.outputFps);
		remember(row.outputBitrateHistory, row.outputBitrate);
		remember(row.encodedFpsHistory, row.encodedFps);
		remember(row.canvasFpsHistory, row.canvasFps);
	}
	emit dataChanged(index(0, 0), index(int(rows_.size()) - 1, ColumnCount - 1),
			 {Qt::DisplayRole, GraphRole, ColorRole});
}

void OutputStatsTable::clear()
{
	if (rows_.isEmpty()) {
		return;
	}
	beginResetModel();
	rows_.clear();
	endResetModel();
}

int OutputStatsTable::rowCount(const QModelIndex &parent) const
{
	return parent.isValid() ? 0 : int(rows_.size());
}

int OutputStatsTable::columnCount(const QModelIndex &parent) const
{
	return parent.isValid() ? 0 : ColumnCount;
}

QVariant OutputStatsTable::value(const StatsRow &row, int column) const
{
	switch (column) {
	case OutputName:
		return row.output ? QVariant(row.outputName) : QVariant();
	case OutputFps:
		return row.outputFps;
	case OutputFrames:
		return row.outputFrames;
	case OutputBitrate:
		return row.outputBitrate;
	case OutputData:
		return qulonglong(row.outputBytes);
	case EncoderName:
		return row.encoder ? QVariant(row.encoderName) : QVariant();
	case EncoderResolution:
		return row.encodedWidth && row.encodedHeight ? QVariant(size(row.encodedWidth, row.encodedHeight)) : QVariant();
	case EncoderFps:
		return row.encodedFps;
	case EncoderFrames:
		return row.encodedFrames;
	case CanvasName:
		return row.hasCanvas ? QVariant(row.canvasName) : QVariant();
	case CanvasResolution:
		return row.canvasWidth && row.canvasHeight ? QVariant(size(row.canvasWidth, row.canvasHeight)) : QVariant();
	case CanvasFps:
		return row.canvasFps;
	case CanvasFrames:
		return row.canvasFrames;
	case CanvasSkipped:
		return row.skippedFrames;
	default:
		return {};
	}
}

QVariant OutputStatsTable::data(const QModelIndex &index, int role) const
{
	if (!index.isValid() || index.row() >= rows_.size() || index.column() >= ColumnCount) {
		return {};
	}
	const auto &row = rows_.at(index.row());
	switch (role) {
	case Qt::DisplayRole:
		return value(row, index.column());
	case Qt::TextAlignmentRole:
		return kColumns[index.column()].alignment;
	case GraphRole:
		switch (index.column()) {
		case OutputFps:
			return QVariant::fromValue(row.outputFpsHistory);
		case OutputBitrate:
			return QVariant::fromValue(row.outputBitrateHistory);
		case EncoderFps:
			return QVariant::fromValue(row.encodedFpsHistory);
		case CanvasFps:
			return QVariant::fromValue(row.canvasFpsHistory);
		default:
			return {};
		}
	case ColorRole:
		return row.canvasColor;
	default:
		return {};
	}
}

QVariant OutputStatsTable::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation != Qt::Horizontal || role != Qt::DisplayRole || section < 0 || section >= ColumnCount) {
		return {};
	}
	auto tr = [this](const char *key) {
		return translate_ ? translate_(key) : QString::fromUtf8(key);
	};
	return tr(kColumns[section].section) + QStringLiteral("\n") + tr(kColumns[section].name);
}

} // namespace fiji::model
