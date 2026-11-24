#include "profile-model.hpp"

#include <QRegularExpression>

#include <QJsonArray>

namespace fiji::model {

namespace {
const char *const kMainKeys[] = {"main_stream_output_show", "main_record_output_show", "main_backtrack_output_show",
				 "main_virtual_cam_output_show"};
const char *const kVertical = "Vertical";
} // namespace

std::pair<QJsonObject, bool> withFirstRunDefaults(QJsonObject doc, bool fileExisted, const QString &recordPath)
{
	if (!fileExisted) { // a new profile shows all of OBS's own outputs
		for (const char *key : kMainKeys) {
			doc.insert(QLatin1String(key), true);
		}
	}
	if (doc.contains(QStringLiteral("canvas"))) {
		return {doc, false};
	}
	// the first time: a Vertical canvas, with a stream, a replay buffer and a virtual camera for it
	doc.insert(QStringLiteral("canvas"), QJsonArray{QJsonObject{{"name", kVertical}, {"color", 0x1F1A17}}});
	QJsonArray outputs = doc.value(QStringLiteral("outputs")).toArray();
	if (outputs.isEmpty()) {
		outputs.append(QJsonObject{{"enabled", true}, {"type", "stream"}, {"name", "Vertical Stream"}, {"canvas", kVertical}});
		QJsonObject backtrack{{"enabled", true},
				      {"type", "backtrack"},
				      {"filename", "%CCYY-%MM-%DD %hh-%mm-%ss Vertical Backtrack"},
				      {"format", "hybrid_mp4"},
				      {"name", "Vertical Backtrack"},
				      {"canvas", kVertical},
				      {"max_time_sec", 10}};
		if (!recordPath.isEmpty()) {
			backtrack.insert(QStringLiteral("path"), recordPath);
		}
		outputs.append(backtrack);
		outputs.append(QJsonObject{{"enabled", true}, {"type", "virtual_cam"}, {"name", "Vertical Virtual Camera"}, {"canvas", kVertical}});
	}
	doc.insert(QStringLiteral("outputs"), outputs);
	return {doc, true};
}

QStringList ProfileModel::ownedKeys()
{
	QStringList keys;
	for (const char *key : kMainKeys) {
		keys.append(QLatin1String(key));
	}
	return keys + hotkeyKeys() + QStringList{"outputs", "canvas"};
}

bool ProfileModel::setCanvases(const QJsonArray &canvases)
{
	if (hasCanvases_ && canvases == canvases_) {
		return false;
	}
	canvases_ = canvases;
	hasCanvases_ = true;
	emit changed();
	return true;
}

QJsonObject ProfileModel::document() const
{
	if (!loaded_) {
		return {};
	}
	QJsonObject out = rest_;
	const auto own = owned();
	for (auto it = own.begin(); it != own.end(); ++it) {
		out.insert(it.key(), it.value());
	}
	return out;
}

void ProfileModel::clear()
{
	main_ = {};
	hotkeys_ = {};
	outputs_ = {};
	hasOutputs_ = false;
	canvases_ = {};
	hasCanvases_ = false;
	view_ = {};
	rest_ = {};
	loaded_ = false;
	emit changed();
}

bool ProfileModel::isViewKey(const QString &key)
{
	static const QRegularExpression encoder(QStringLiteral("^video_encoder_description\\d+$"));
	return key == QLatin1String("stats_state") || key.startsWith(QLatin1String("stats_state_")) ||
	       key.contains(QLatin1String("_canvas_split")) || key == QLatin1String("preview_disabled") ||
	       key == QLatin1String("preview_locked") || key == QLatin1String("config_geometry") ||
	       encoder.match(key).hasMatch();
}

bool ProfileModel::setView(const QString &key, const QJsonValue &value)
{
	if (!isViewKey(key) || view_.value(key) == value) {
		return false;
	}
	if (value.isUndefined()) {
		view_.remove(key);
	} else {
		view_.insert(key, value);
	}
	emit changed();
	return true;
}

QJsonObject ProfileModel::canvas(const QString &name) const
{
	if (name.isEmpty()) {
		return {};
	}
	for (const auto &value : canvases_) {
		const auto canvas = value.toObject();
		if (canvas.value(QStringLiteral("name")).toString() == name) {
			return canvas;
		}
	}
	return {};
}

bool ProfileModel::updateCanvas(const QJsonObject &canvas)
{
	const QString uuid = canvas.value(QStringLiteral("uuid")).toString();
	const QString name = canvas.value(QStringLiteral("name")).toString();
	for (int i = 0; i < canvases_.size(); i++) {
		const auto current = canvases_.at(i).toObject();
		const QString currentUuid = current.value(QStringLiteral("uuid")).toString();
		const bool same = !uuid.isEmpty() && !currentUuid.isEmpty() ? uuid == currentUuid
									      : name == current.value(QStringLiteral("name")).toString();
		if (same) {
			if (current == canvas) {
				return false;
			}
			canvases_.replace(i, canvas);
			emit changed();
			return true;
		}
	}
	return false;
}

bool ProfileModel::setOutputs(const QJsonArray &outputs)
{
	if (hasOutputs_ && outputs == outputs_) {
		return false;
	}
	outputs_ = outputs;
	hasOutputs_ = true;
	emit changed();
	return true;
}

bool ProfileModel::updateOutput(const QString &name, const QJsonObject &output)
{
	for (int i = 0; i < outputs_.size(); i++) {
		if (outputs_.at(i).toObject().value(QStringLiteral("name")).toString() == name) {
			if (outputs_.at(i).toObject() == output) {
				return false;
			}
			outputs_.replace(i, output);
			emit changed();
			return true;
		}
	}
	return false;
}

QStringList ProfileModel::hotkeyKeys()
{
	return {"start_all_outputs_hotkey",   "stop_all_outputs_hotkey",       "start_all_streams_hotkey",
		"start_all_recordings_hotkey", "multiview_over_preview_hotkey", "group_hotkeys"};
}

bool ProfileModel::setHotkey(const QString &key, const QJsonValue &value)
{
	if (!hotkeyKeys().contains(key) || hotkeys_.value(key) == value) {
		return false;
	}
	hotkeys_.insert(key, value);
	emit changed();
	return true;
}

void ProfileModel::load(const QJsonObject &document)
{
	// a file from before a key existed: off, as OBS's obs_data reads a missing bool
	main_ = {document.value(QLatin1String(kMainKeys[0])).toBool(false), document.value(QLatin1String(kMainKeys[1])).toBool(false),
		 document.value(QLatin1String(kMainKeys[2])).toBool(false), document.value(QLatin1String(kMainKeys[3])).toBool(false)};
	view_ = {};
	rest_ = {};
	const auto owns = ownedKeys();
	for (auto it = document.begin(); it != document.end(); ++it) {
		if (isViewKey(it.key())) {
			view_.insert(it.key(), it.value());
		} else if (!owns.contains(it.key())) {
			rest_.insert(it.key(), it.value());
		}
	}
	loaded_ = true;
	hasCanvases_ = document.contains(QStringLiteral("canvas"));
	canvases_ = document.value(QStringLiteral("canvas")).toArray();
	hasOutputs_ = document.contains(QStringLiteral("outputs"));
	outputs_ = document.value(QStringLiteral("outputs")).toArray();
	hotkeys_ = {};
	for (const auto &key : hotkeyKeys()) {
		if (document.contains(key)) {
			hotkeys_.insert(key, document.value(key));
		}
	}
	emit changed();
}

QJsonObject ProfileModel::owned() const
{
	QJsonObject out = hotkeys_; // only the bindings that exist: a profile without them stays without them
	if (hasOutputs_) {
		out.insert(QStringLiteral("outputs"), outputs_);
	}
	if (hasCanvases_) {
		out.insert(QStringLiteral("canvas"), canvases_);
	}
	for (auto it = view_.begin(); it != view_.end(); ++it) {
		out.insert(it.key(), it.value());
	}
	out.insert(QLatin1String(kMainKeys[0]), main_.stream);
	out.insert(QLatin1String(kMainKeys[1]), main_.record);
	out.insert(QLatin1String(kMainKeys[2]), main_.backtrack);
	out.insert(QLatin1String(kMainKeys[3]), main_.virtualCam);
	return out;
}

bool ProfileModel::setMainOutputs(const MainOutputs &outputs)
{
	if (outputs == main_) {
		return false;
	}
	main_ = outputs;
	emit changed();
	return true;
}

} // namespace fiji::model
