#include "output-stats.hpp"

#include <algorithm>

namespace fiji::model {

QString outputDisplayName(const QString &obsName, const StatsNames &names)
{
	QString name = obsName;
	if (name.startsWith(QStringLiteral("Fiji Suite Output "))) {
		name.replace(QStringLiteral("Fiji Suite Output "), QString());
	} else if (name.startsWith(QStringLiteral("fiji_multi_output_"))) {
		name.replace(QStringLiteral("fiji_multi_output_"), QString());
	} else if (name == "simple_file_output" || name == "adv_file_output" || name == "simple_ffmpeg_output" ||
		   name == "adv_ffmpeg_output") {
		name = names.record;
	} else if (name == "simple_stream" || name == "adv_stream" || name == "rtmp multitrack video") {
		name = names.stream;
	} else if (name == "virtualcam_output") {
		name = names.virtualCamera;
	} else {
		name.replace('_', ' '); // "mp4 multitrack video", decklink_output, aja_output, ...
	}
	return name;
}

QString encoderDisplayName(const QString &obsName)
{
	QString name = obsName;
	if (name.startsWith(QStringLiteral("Fiji Suite Video "))) {
		name.replace(QStringLiteral("Fiji Suite Video "), QString());
	} else if (name.startsWith(QStringLiteral("Fiji Suite Audio "))) {
		name.replace(QStringLiteral("Fiji Suite Audio "), QString());
	} else {
		name.replace('_', ' ');
	}
	return name;
}

OutputStats::OutputStats(StatsNames names) : names_(std::move(names)) {}

void OutputStats::clear()
{
	rows_.clear();
}

void OutputStats::changed(int row)
{
	first_ = first_ < 0 ? row : std::min(first_, row);
	last_ = std::max(last_, row);
}

// A new row starts from the counters as they are, so its first rates are not the whole total.
StatsRow OutputStats::newRow(quintptr output, const QString &outputName, const EncoderSample *encoder, quintptr video)
{
	StatsRow row;
	row.output = output;
	row.outputName = outputName;
	row.video = video;
	row.updated = true;
	if (encoder) {
		row.encoder = encoder->id;
		row.encoderName = encoderDisplayName(encoder->name);
		row.encodedWidth = encoder->width;
		row.encodedHeight = encoder->height;
		row.encodedFrames = encoder->frames;
	}
	return row;
}

static quint32 perSecond(quint64 now, quint64 before)
{
	return now > before ? static_cast<quint32>(now - before) : 0;
}

StatsUpdate OutputStats::update(const StatsSnapshot &snapshot)
{
	first_ = last_ = -1;
	const int before = static_cast<int>(rows_.size());
	for (auto &row : rows_) {
		row.updated = false;
	}

	// encoders: the rows that encode with them, or a new row for an active one
	for (const auto &encoder : snapshot.encoders) {
		bool found = false;
		for (int i = 0; i < rows_.size(); i++) {
			auto &row = rows_[i];
			if (row.encoder == encoder.id) {
				found = true;
				row.encodedFps = perSecond(encoder.frames, row.encodedFrames);
				row.encodedFrames = encoder.frames;
				row.updated = true;
				changed(i);
			}
		}
		if (!found && encoder.active) {
			changed(static_cast<int>(rows_.size()));
			rows_.append(newRow(0, QString(), &encoder, encoder.video));
		}
	}

	// outputs: claim their encoders' rows, update theirs, or add rows of their own
	for (const auto &output : snapshot.outputs) {
		bool found = false;
		for (int i = 0; i < rows_.size(); i++) {
			auto &row = rows_[i];
			if (output.encoded && output.active && !row.output && row.encoder) {
				for (const auto &encoder : output.videoEncoders) {
					if (encoder.id == row.encoder) {
						row.output = output.id;
						row.outputName = outputDisplayName(output.name, names_);
						row.outputBytes = output.bytes;
						row.outputFrames = output.frames;
						changed(i);
						break;
					}
				}
			}
			if (row.output == output.id) {
				row.activeDelay = output.delay;
				row.droppedFrames = output.dropped;
				row.outputBitrate = static_cast<quint32>(perSecond(output.bytes, row.outputBytes) * 8 / 1000);
				row.outputBytes = output.bytes;
				row.outputFps = perSecond(output.frames, row.outputFrames);
				row.outputFrames = output.frames;
				row.updated = true;
				changed(i);
				found = true;
			}
		}
		if (found || !output.active) {
			continue;
		}
		const QString name = outputDisplayName(output.name, names_);
		auto start = [&] {
			auto row = rows_.size();
			changed(static_cast<int>(row));
			return row;
		};
		if (output.encoded) {
			for (const auto &encoder : output.videoEncoders) {
				start();
				auto row = newRow(output.id, name, &encoder, encoder.video);
				row.outputBytes = output.bytes;
				row.outputFrames = output.frames;
				rows_.append(row);
			}
		} else {
			start();
			auto row = newRow(output.id, name, nullptr, output.video);
			row.outputBytes = output.bytes;
			row.outputFrames = output.frames;
			rows_.append(row);
		}
	}

	// canvases: name and colour the rows that encode their video, and their frame counts
	if (!rows_.isEmpty()) {
		for (const auto &canvas : snapshot.canvases) {
			for (int i = 0; i < rows_.size(); i++) {
				auto &row = rows_[i];
				if (!row.canvas && row.video && row.video == canvas.video) {
					row.canvas = canvas.id;
					row.canvasName = canvas.name;
					row.canvasColor = canvas.color;
					row.canvasFrames = canvas.frames;
					changed(i);
				}
				if (row.canvas == canvas.id || row.video == canvas.video) {
					row.skippedFrames = canvas.skipped;
					row.canvasFps = perSecond(canvas.frames, row.canvasFrames);
					row.canvasFrames = canvas.frames;
					if (canvas.width && canvas.height) {
						row.canvasWidth = canvas.width;
						row.canvasHeight = canvas.height;
					}
					changed(i);
				}
			}
		}
	}

	// rows nothing updated: their rates fall to zero
	for (int i = 0; i < rows_.size(); i++) {
		auto &row = rows_[i];
		if (!row.updated && (row.encodedFps || row.outputBitrate || row.outputFps || row.canvasFps)) {
			row.encodedFps = row.outputBitrate = row.outputFps = row.canvasFps = 0;
			changed(i);
		}
	}

	StatsUpdate result;
	result.inserted = static_cast<int>(rows_.size()) - before;
	result.firstChanged = first_;
	result.lastChanged = last_;
	return result;
}

} // namespace fiji::model
