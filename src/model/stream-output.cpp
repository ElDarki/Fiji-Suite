#include "stream-output.hpp"

#include <QJsonArray>

namespace fiji::model {

QList<StreamPlatform> streamPlatforms()
{
	using S = StreamPlatform::Server;
	return {
		{"twitch", "Twitch", ":/fiji/media/twitch.png", "Twitch", S::Choice, {}, "Twitch", false},
		{"youtube", "YouTube", ":/fiji/media/youtube.png", "YouTube", S::Choice, {}, "YouTube - RTMPS", false},
		{"tiktok", "TikTok", ":/fiji/media/tiktok.png", "TikTok", S::Free, {}, {}, false},
		{"facebook", "Facebook", ":/fiji/media/facebook.png", "Facebook", S::Free, "rtmps://rtmp-api.facebook.com:443/rtmp/", {}, false},
		{"trovo", "Trovo", ":/fiji/media/trovo.png", "Trovo", S::Fixed, "rtmp://livepush.trovo.live/live/", {}, false},
		{"twitter", "X (Twitter)", ":/fiji/media/twitter.png", "Twitter", S::Choice, {}, "Twitter", false},
		{"kick", "Kick", ":/fiji/media/kick.png", "Kick", S::Fixed, "rtmps://fa723fc1b171.global-contribute.live-video.net", {}, true},
		{"custom", "", ":/fiji/media/unknown.png", "Custom", S::Free, {}, {}, false},
	};
}

StreamPlatform streamPlatform(const QString &id)
{
	for (const auto &platform : streamPlatforms()) {
		if (platform.id == id) {
			return platform;
		}
	}
	return streamPlatforms().last();
}

bool isTwitchServer(const QString &server)
{
	return server.contains("ingest.global-contribute.live-video.net") || server.contains(".contribute.live-video.net") ||
	       server.contains(".twitch.tv");
}

QString streamPlatformFor(const QString &server)
{
	if (isTwitchServer(server)) {
		return QStringLiteral("twitch");
	}
	if (server.contains(".youtube.com"))
		return QStringLiteral("youtube");
	if (server.contains("fa723fc1b171.global-contribute.live-video.net"))
		return QStringLiteral("kick");
	if (server.contains(".tiktokcdn-"))
		return QStringLiteral("tiktok");
	if (server.contains(".pscp.tv"))
		return QStringLiteral("twitter");
	if (server.contains("livepush.trovo.live"))
		return QStringLiteral("trovo");
	if (server.contains(".fbcdn.net") || server.contains(".facebook.com"))
		return QStringLiteral("facebook");
	return QStringLiteral("custom");
}

QList<StreamServer> serversOf(const QJsonObject &servicesJson, const QString &service)
{
	QList<StreamServer> out;
	for (const auto &value : servicesJson.value("services").toArray()) {
		const auto entry = value.toObject();
		if (entry.value("name").toString() != service) {
			continue;
		}
		for (const auto &server : entry.value("servers").toArray()) {
			out.append({server.toObject().value("name").toString(), server.toObject().value("url").toString()});
		}
		break;
	}
	return out;
}

QList<StreamServer> twitchIngests(const QJsonObject &ingestsJson)
{
	QList<StreamServer> out{{QStringLiteral("Default"), QStringLiteral("rtmp://live.twitch.tv/app")}};
	for (const auto &value : ingestsJson.value("ingests").toArray()) {
		QString url = value.toObject().value("url_template").toString();
		url.remove(QStringLiteral("/{stream_key}"));
		out.append({value.toObject().value("name").toString(), url});
	}
	return out;
}

bool serverWithoutKey(const QString &server)
{
	return server.contains("whip", Qt::CaseInsensitive) || server.startsWith("srt://", Qt::CaseInsensitive) ||
	       server.startsWith("rist://", Qt::CaseInsensitive);
}

bool streamOutputValid(const StreamOutput &output, const QStringList &otherNames)
{
	if (output.name.isEmpty() || otherNames.contains(output.name) || output.server.isEmpty()) {
		return false;
	}
	return !output.key.isEmpty() || serverWithoutKey(output.server);
}

} // namespace fiji::model
