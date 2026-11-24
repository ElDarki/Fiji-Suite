#include "profile-model.hpp"

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
	return keys + hotkeyKeys();
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
