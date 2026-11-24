#include "obs-data-store.hpp"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QSaveFile>

namespace fiji::obs {

ObsDataStore::ObsDataStore(QString folder, std::function<void(const QString &)> log)
	: folder_(std::move(folder)),
	  log_(std::move(log))
{
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
		if (log_) {
			log_(QStringLiteral("%1.json could not be read (%2); it is left untouched").arg(key, error.errorString()));
		}
		return {};
	}
	return document.object();
}

bool ObsDataStore::save(const QString &key, const QJsonObject &value)
{
	const QString target = path(key);
	QDir().mkpath(folder_);
	if (QFile::exists(target)) { // keep the previous file as a backup before replacing it
		QFile::remove(target + QStringLiteral(".bak"));
		QFile::copy(target, target + QStringLiteral(".bak"));
	}
	QSaveFile file(target);
	if (!file.open(QIODevice::WriteOnly) || file.write(QJsonDocument(value).toJson()) < 0 || !file.commit()) {
		if (log_) {
			log_(QStringLiteral("%1.json could not be saved").arg(key));
		}
		return false;
	}
	return true;
}

} // namespace fiji::obs
