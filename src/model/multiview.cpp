#include "multiview.hpp"

#include <QTime>

#include <algorithm>
#include <cmath>

namespace fiji::model {

bool outputOnTile(const MultiviewOutput &output, const QString &canvasName, bool mainTile)
{
	if (output.main || output.canvas.isEmpty()) {
		return mainTile;
	}
	return !mainTile && output.canvas == canvasName;
}

int multiviewColumns(int tiles)
{
	return std::max(1, int(std::ceil(std::sqrt(double(tiles)))));
}

QString liveTime(qint64 seconds)
{
	const auto t = QTime(0, 0).addSecs(int(std::max<qint64>(0, seconds) % 86400));
	return t.toString(t.hour() ? QStringLiteral("hh:mm:ss") : QStringLiteral("mm:ss"));
}

TileOutputs::TileOutputs(QString canvasName, bool mainTile) : canvasName_(std::move(canvasName)), mainTile_(mainTile) {}

TileOutputs::Update TileOutputs::update(const QList<MultiviewOutput> &outputs, qint64 nowMs)
{
	Update result;
	QStringList present;
	for (const auto &o : outputs) {
		if (!outputOnTile(o, canvasName_, mainTile_)) {
			continue;
		}
		present.append(o.name);

		auto found = rates_.find(o.name);
		if (found == rates_.end()) {
			found = rates_.insert(o.name, Rate{o.totalBytes, nowMs, 0});
		}
		auto &rate = found.value();
		if (o.active && nowMs > rate.time) {
			if (o.totalBytes >= rate.bytes) {
				rate.kbps = int((o.totalBytes - rate.bytes) * 8 / quint64(nowMs - rate.time));
			}
		} else if (!o.active) {
			rate.kbps = 0;
		}
		rate.bytes = o.totalBytes;
		rate.time = nowMs;

		Chip chip;
		chip.name = o.name;
		chip.type = o.type;
		chip.server = o.server;
		if (o.reconnecting) {
			chip.state = ChipState::Reconnecting;
			chip.warning = true;
		} else if (o.active) {
			chip.state = ChipState::Live;
			result.live = true;
			chip.liveSeconds = o.liveSeconds;
			chip.showRate = o.type == QStringLiteral("stream") || o.type == QStringLiteral("ffmpeg");
			chip.kbps = rate.kbps;
			if (o.totalFrames > 0 && o.droppedFrames > 0) {
				chip.droppedPercent = 100.0 * double(o.droppedFrames) / double(o.totalFrames);
				chip.warning = chip.droppedPercent >= droppedWarningPercent;
			}
		}
		result.chips.append(chip);
	}
	for (auto it = rates_.begin(); it != rates_.end();) {
		if (present.contains(it.key())) {
			++it;
		} else {
			result.gone.append(it.key());
			it = rates_.erase(it);
		}
	}
	return result;
}

} // namespace fiji::model
