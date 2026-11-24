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
	return keys;
}

void ProfileModel::load(const QJsonObject &document)
{
	// a file from before a key existed: off, as OBS's obs_data reads a missing bool
	main_ = {document.value(QLatin1String(kMainKeys[0])).toBool(false), document.value(QLatin1String(kMainKeys[1])).toBool(false),
		 document.value(QLatin1String(kMainKeys[2])).toBool(false), document.value(QLatin1String(kMainKeys[3])).toBool(false)};
	emit changed();
}

QJsonObject ProfileModel::owned() const
{
	return {{QLatin1String(kMainKeys[0]), main_.stream},
		{QLatin1String(kMainKeys[1]), main_.record},
		{QLatin1String(kMainKeys[2]), main_.backtrack},
		{QLatin1String(kMainKeys[3]), main_.virtualCam}};
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
