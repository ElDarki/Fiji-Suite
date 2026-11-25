#include "obs-data-store.hpp"

#include <obs-module.h>

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QSaveFile>

namespace fiji::obs {

ObsDataStore::ObsDataStore(QString folder) : folder_(std::move(folder)) {}

QString ObsDataStore::configFolder()
{
	char *path = obs_module_config_path("");
	const QString folder = QString::fromUtf8(path);
	bfree(path);
	return folder;
}

QString ObsDataStore::path(const QString &key) const
{
	return QDir(folder_).filePath(key + QStringLiteral(".json"));
}

QJsonObject ObsDataStore::load(const QString &key) const
{
	QFile file(path(key));
	if (!file.open(QIODevice::ReadOnly)) {
		return {}; // nothing saved yet
	}
	QJsonParseError error;
	const auto document = QJsonDocument::fromJson(file.readAll(), &error);
	if (error.error != QJsonParseError::NoError || !document.isObject()) {
		blog(LOG_WARNING, "[Fiji Suite] %s.json could not be read (%s); it is left untouched until the next save",
		     key.toUtf8().constData(), error.errorString().toUtf8().constData());
		return {};
	}
	return document.object();
}

bool ObsDataStore::save(const QString &key, const QJsonObject &value)
{
	const QString file = path(key);
	QDir().mkpath(folder_);
	if (QFile::exists(file)) {
		QFile::remove(file + QStringLiteral(".bak"));
		QFile::copy(file, file + QStringLiteral(".bak"));
	}
	QSaveFile out(file);
	if (!out.open(QIODevice::WriteOnly) || out.write(QJsonDocument(value).toJson()) < 0 || !out.commit()) {
		blog(LOG_WARNING, "[Fiji Suite] %s.json could not be saved", key.toUtf8().constData());
		return false;
	}
	return true;
}

} // namespace fiji::obs
