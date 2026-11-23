#include "local-server.hpp"

#include "../utils/bot-link.hpp"
#include <atomic>
#include <obs-frontend-api.h>
#include <obs-module.h>
#include <obs.hpp>
#include <QFile>
#include <QNetworkInterface>
#include <QRegularExpression>
#include <QUuid>
#include <util/config-file.h>

static LocalServer *instance = nullptr;
// read by fiji_events_wanted() from any thread
static std::atomic<int> listenerCount{0};

static const int defaultPort = 28190;
static const qsizetype maxHeader = 16 * 1024;
static const qsizetype maxBody = 4 * 1024 * 1024;

static QByteArray content_type(const QString &name)
{
	if (name.endsWith(QStringLiteral(".html"))) {
		return "text/html; charset=utf-8";
	} else if (name.endsWith(QStringLiteral(".js"))) {
		return "text/javascript; charset=utf-8";
	} else if (name.endsWith(QStringLiteral(".css"))) {
		return "text/css; charset=utf-8";
	} else if (name.endsWith(QStringLiteral(".json"))) {
		return "application/json";
	} else if (name.endsWith(QStringLiteral(".svg"))) {
		return "image/svg+xml";
	} else if (name.endsWith(QStringLiteral(".png"))) {
		return "image/png";
	} else if (name.endsWith(QStringLiteral(".woff2"))) {
		return "font/woff2";
	}
	return "application/octet-stream";
}

// A file of the web interface by its plain name; empty when there is no such file. Names with
// anything but letters, digits, dot, dash and underscore are refused, so nothing outside
// data/web can be reached.
static QByteArray web_file(const QString &name)
{
	static const QRegularExpression plain(QStringLiteral("^[A-Za-z0-9_-][A-Za-z0-9._-]*$"));
	if (!plain.match(name).hasMatch()) {
		return {};
	}
	char *path = obs_module_file(QStringLiteral("web/%1").arg(name).toUtf8().constData());
	if (!path) {
		return {};
	}
	QFile file(QString::fromUtf8(path));
	bfree(path);
	return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

// The bot shows the same interface by asking the plugin for its files.
void vendor_request_get_web_file(obs_data_t *request, obs_data_t *response, void *)
{
	const QString name = QString::fromUtf8(obs_data_get_string(request, "name"));
	const QByteArray content = web_file(name);
	if (content.isEmpty()) {
		obs_data_set_string(response, "error", "'name' not found");
		obs_data_set_bool(response, "success", false);
		return;
	}
	obs_data_set_string(response, "content", content.constData());
	obs_data_set_string(response, "type", content_type(name).constData());
	obs_data_set_bool(response, "success", true);
}

LocalServer *LocalServer::Get()
{
	return instance;
}

void LocalServer::Start()
{
	Stop();
	auto config = obs_frontend_get_user_config();
	if (!config) {
		return;
	}
	config_set_default_bool(config, "FijiSuite", "DeckServer", true);
	config_set_default_int(config, "FijiSuite", "DeckPort", defaultPort);
	config_set_default_bool(config, "FijiSuite", "DeckNetwork", false);
	if (!config_get_bool(config, "FijiSuite", "DeckServer")) {
		return;
	}
	instance = new LocalServer;
	const char *saved = config_get_string(config, "FijiSuite", "DeckToken");
	instance->token = QString::fromUtf8(saved ? saved : "");
	if (instance->token.length() < 16) {
		instance->token = QUuid::createUuid().toString(QUuid::Id128);
		config_set_string(config, "FijiSuite", "DeckToken", instance->token.toUtf8().constData());
		config_save_safe(config, "tmp", "bak");
	}
	instance->network = config_get_bool(config, "FijiSuite", "DeckNetwork");
	const auto port = quint16(config_get_int(config, "FijiSuite", "DeckPort"));
	if (!instance->server.listen(instance->network ? QHostAddress::Any : QHostAddress::LocalHost, port)) {
		blog(LOG_WARNING, "[Fiji Suite] The deck server could not listen on port %d: %s", int(port),
		     instance->server.errorString().toUtf8().constData());
		Stop();
		return;
	}
	blog(LOG_INFO, "[Fiji Suite] Deck server listening on port %d (%s)", int(port),
	     instance->network ? "local network" : "this computer only");
}

void LocalServer::Stop()
{
	delete instance;
	instance = nullptr;
	listenerCount = 0;
}

void LocalServer::ResetToken()
{
	if (auto config = obs_frontend_get_user_config()) {
		config_set_string(config, "FijiSuite", "DeckToken", "");
		config_save_safe(config, "tmp", "bak");
	}
	Start();
}

LocalServer::LocalServer()
{
	connect(&server, &QTcpServer::newConnection, this, [this] { Accept(); });
}

LocalServer::~LocalServer()
{
	server.close();
	for (auto socket : findChildren<QTcpSocket *>()) {
		socket->abort();
	}
}

QString LocalServer::LocalAddress() const
{
	return server.isListening() ? QStringLiteral("http://127.0.0.1:%1/%2/").arg(server.serverPort()).arg(token) : QString();
}

QString LocalServer::NetworkAddress() const
{
	if (!server.isListening() || !network) {
		return {};
	}
	for (const auto &address : QNetworkInterface::allAddresses()) {
		if (address.protocol() == QAbstractSocket::IPv4Protocol && !address.isLoopback()) {
			return QStringLiteral("http://%1:%2/%3/").arg(address.toString()).arg(server.serverPort()).arg(token);
		}
	}
	return {};
}

void LocalServer::Accept()
{
	while (auto socket = server.nextPendingConnection()) {
		socket->setParent(this);
		connect(socket, &QTcpSocket::readyRead, this, [this, socket] { Read(socket); });
		connect(socket, &QTcpSocket::disconnected, this, [this, socket] {
			if (listeners.removeAll(socket)) {
				listenerCount = int(listeners.size());
			}
			socket->deleteLater();
		});
	}
}

void LocalServer::Read(QTcpSocket *socket)
{
	// the request so far is kept on the socket until it is complete
	QByteArray buffer = socket->property("request").toByteArray() + socket->readAll();
	const qsizetype headerEnd = buffer.indexOf("\r\n\r\n");
	if (headerEnd < 0) {
		if (buffer.size() > maxHeader) {
			socket->abort();
			return;
		}
		socket->setProperty("request", buffer);
		return;
	}
	const QList<QByteArray> lines = buffer.left(headerEnd).split('\n');
	const QList<QByteArray> requestLine = lines.value(0).trimmed().split(' ');
	qsizetype length = 0;
	for (const auto &line : lines) {
		if (line.toLower().startsWith("content-length:")) {
			length = line.mid(15).trimmed().toLongLong();
		}
	}
	if (requestLine.size() < 2 || length < 0 || length > maxBody) {
		Reply(socket, 400, "text/plain", "Bad request");
		return;
	}
	const QByteArray body = buffer.mid(headerEnd + 4);
	if (body.size() < length) {
		socket->setProperty("request", buffer);
		return;
	}
	socket->setProperty("request", QByteArray());
	Handle(socket, requestLine.at(0), requestLine.at(1), body.left(length));
}

void LocalServer::Reply(QTcpSocket *socket, int status, const QByteArray &type, const QByteArray &body)
{
	const char *reason = status == 200 ? "OK" : status == 400 ? "Bad Request" : status == 405 ? "Method Not Allowed" : "Not Found";
	QByteArray head = "HTTP/1.1 " + QByteArray::number(status) + " " + reason + "\r\nContent-Type: " + type +
			  "\r\nContent-Length: " + QByteArray::number(body.size()) +
			  "\r\nCache-Control: no-cache\r\nConnection: close\r\n\r\n";
	socket->write(head + body);
	socket->disconnectFromHost();
}

void LocalServer::Handle(QTcpSocket *socket, const QByteArray &method, const QByteArray &rawPath, const QByteArray &body)
{
	const QString path = QString::fromUtf8(QByteArray::fromPercentEncoding(rawPath)).section('?', 0, 0);
	const QString prefix = QStringLiteral("/%1/").arg(token);
	if (!path.startsWith(prefix)) {
		Reply(socket, 404, "text/plain", "Not found");
		return;
	}
	const QString rest = path.mid(prefix.length());

	if (rest == QStringLiteral("api")) {
		if (method != "POST") {
			Reply(socket, 405, "text/plain", "Use POST");
			return;
		}
		OBSDataAutoRelease message = obs_data_create_from_json(body.constData());
		if (!message) {
			Reply(socket, 400, "text/plain", "The body is not JSON");
			return;
		}
		OBSDataAutoRelease request = obs_data_get_obj(message, "data");
		if (!request) {
			request = obs_data_create();
		}
		OBSDataAutoRelease response = obs_data_create();
		if (!fiji_call_request(obs_data_get_string(message, "request"), request, response)) {
			obs_data_set_string(response, "error", "unknown request");
			obs_data_set_bool(response, "success", false);
		}
		Reply(socket, 200, "application/json", obs_data_get_json(response));
		return;
	}

	if (rest == QStringLiteral("events")) {
		socket->write("HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nCache-Control: no-cache\r\n"
			      "Connection: keep-alive\r\n\r\n: connected\n\n");
		listeners.append(socket);
		listenerCount = int(listeners.size());
		return;
	}

	const QString name = rest.isEmpty() ? QStringLiteral("board.html") : rest;
	const QByteArray content = web_file(name);
	if (content.isEmpty()) {
		Reply(socket, 404, "text/plain", "Not found");
	} else {
		Reply(socket, 200, content_type(name), content);
	}
}

void LocalServer::Event(const char *event, obs_data_t *data)
{
	if (listenerCount <= 0) {
		return;
	}
	OBSDataAutoRelease message = obs_data_create();
	obs_data_set_string(message, "event", event);
	if (data) {
		obs_data_set_obj(message, "data", data);
	}
	// one line of JSON per event, as Server-Sent Events expects
	const QByteArray payload = "data: " + QByteArray(obs_data_get_json(message)).replace('\n', ' ') + "\n\n";
	QMetaObject::invokeMethod(
		this,
		[this, payload] {
			for (auto socket : listeners) {
				socket->write(payload);
			}
		},
		Qt::QueuedConnection);
}

bool local_server_has_listeners()
{
	return listenerCount > 0;
}
