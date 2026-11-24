#include "stream-service-files.hpp"

#include <obs.hpp>
#include <obs-module.h>
#include <util/platform.h>

#include <QFile>
#include <QJsonDocument>

namespace fiji::obs {

namespace {
QJsonObject readRtmpServicesFile(const char *name)
{
	obs_module_t *module = obs_get_module("rtmp-services");
	char *path = module ? obs_module_get_config_path(module, name) : nullptr;
	char *absolute = path ? os_get_abs_path_ptr(path) : nullptr;
	QJsonObject out;
	if (absolute) {
		QFile file(QString::fromUtf8(absolute));
		if (file.open(QIODevice::ReadOnly)) {
			out = QJsonDocument::fromJson(file.readAll()).object();
		}
	}
	bfree(absolute);
	bfree(path);
	return out;
}
} // namespace

QList<model::StreamServer> StreamServersFor(const QString &service)
{
	QList<model::StreamServer> out;
	if (service == QLatin1String("Twitch")) {
		out = model::twitchIngests(readRtmpServicesFile("twitch_ingests.json"));
	}
	out += model::serversOf(readRtmpServicesFile("services.json"), service);
	return out;
}

} // namespace fiji::obs
