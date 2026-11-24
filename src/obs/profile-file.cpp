#include "profile-file.hpp"

#include <obs-frontend-api.h>
#include <obs.hpp>
#include <util/config-file.h>

#include <QDir>
#include <QJsonDocument>

namespace fiji::obs {

QString CurrentProfileFile()
{
	char *folder = obs_frontend_get_current_profile_path();
	if (!folder) {
		return {};
	}
	const QString path = QDir(QString::fromUtf8(folder)).filePath(QStringLiteral("fiji-suite.json"));
	bfree(folder);
	return path;
}

std::optional<QJsonObject> LoadProfileFile(const QString &path)
{
	if (path.isEmpty()) {
		return std::nullopt;
	}
	OBSDataAutoRelease data = obs_data_create_from_json_file_safe(path.toUtf8().constData(), "bak");
	if (!data) {
		return std::nullopt;
	}
	return QJsonDocument::fromJson(QByteArray(obs_data_get_json(data))).object();
}

bool SaveProfileFile(const QString &path, const QJsonObject &document)
{
	if (path.isEmpty()) {
		return false;
	}
	OBSDataAutoRelease data = obs_data_create_from_json(QJsonDocument(document).toJson(QJsonDocument::Compact).constData());
	return data && obs_data_save_json_safe(data, path.toUtf8().constData(), "tmp", "bak");
}

QString ProfileRecordPath()
{
	config_t *config = obs_frontend_get_profile_config();
	if (!config) {
		return {};
	}
	const char *mode = config_get_string(config, "Output", "Mode");
	const char *path = nullptr;
	if (mode && strcmp(mode, "Advanced") == 0) {
		path = config_get_string(config, "AdvOut", "RecFilePath");
	}
	if (!path || path[0] == '\0') {
		path = config_get_string(config, "SimpleOutput", "FilePath");
	}
	return QString::fromUtf8(path ? path : "");
}

} // namespace fiji::obs
