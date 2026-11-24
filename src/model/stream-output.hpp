#pragma once

// MODEL: a Fiji Suite stream output as its dialog edits it ({"name", "stream_server", "stream_key"} in the profile's
// output list), the platforms the dialog offers, and its rules. Qt Core only.

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

namespace fiji::model {

struct StreamOutput {
	QString name;
	QString server;
	QString key;
};

struct StreamServer {
	QString name;
	QString url;
	bool operator==(const StreamServer &other) const { return name == other.name && url == other.url; }
};

// One platform page of the dialog. Its text keys are built from `keys`: "<keys>ServiceInfo", "<keys>Output" (the
// default name), "<keys>Server", "<keys>ServerInfo", "<keys>StreamKey", "<keys>StreamKeyInfo".
struct StreamPlatform {
	enum class Server {
		Fixed,  // `server` and nothing else
		Choice, // a list: OBS's servers for the service named `service`
		Free,   // typed in; `server` is the default for a new output
	};
	QString id;
	QString label; // the selection button ("" = the translated "OtherService")
	QString icon;  // a Qt resource
	QString keys;
	Server serverKind = Server::Free;
	QString server;
	QString service;
	bool editTitle = false; // has its own "<keys>ServiceInfoEdit" when editing
};

// The platforms in the order the selection page shows them (two rows of four).
QList<StreamPlatform> streamPlatforms();
// The platform an existing output's server belongs to ("custom" when none).
QString streamPlatformFor(const QString &server);
StreamPlatform streamPlatform(const QString &id);

// OBS's servers for a service, from rtmp-services' services.json ({"services": [{"name", "servers": [{"name","url"}]}]}).
QList<StreamServer> serversOf(const QJsonObject &servicesJson, const QString &service);
// Twitch's ingests, from rtmp-services' twitch_ingests.json, after Twitch's default server.
QList<StreamServer> twitchIngests(const QJsonObject &ingestsJson);

// A server that needs no stream key: WHIP, SRT and RIST.
bool serverWithoutKey(const QString &server);
// Whether the dialog may save: a name no other output has, a server, and a key unless the server needs none.
bool streamOutputValid(const StreamOutput &output, const QStringList &otherNames);

} // namespace fiji::model
