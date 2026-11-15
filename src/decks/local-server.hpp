#pragma once

#include <obs.h>
#include <QByteArray>
#include <QList>
#include <QObject>
#include <QString>
#include <QTcpServer>
#include <QTcpSocket>

// Serves the deck web interface (data/web) from inside OBS, so decks open in a dock, a browser
// or a phone on the same network with no bot and no internet.
//
//   GET  /<token>/            board.html
//   GET  /<token>/<file>      a file from data/web
//   POST /<token>/api         {"request": "...", "data": {...}} -> the request's response
//   GET  /<token>/events      Server-Sent Events: one JSON {"event", "data"} per plugin event
//
// The token is a secret kept in OBS's user config; without it every path answers 404. The server
// listens on this computer only unless the local network is allowed in the settings.
class LocalServer : public QObject {
	Q_OBJECT
public:
	static void Start();
	static void Stop();
	static LocalServer *Get();

	// http://127.0.0.1:<port>/<token>/ ; empty when the server is not running.
	QString LocalAddress() const;
	// The same with this computer's network address, when the local network is allowed.
	QString NetworkAddress() const;
	bool HasListeners() const { return !listeners.isEmpty(); }
	// Sends an event to every open page. Call from any thread.
	void Event(const char *event, obs_data_t *data);

	// Replaces the token (old addresses stop working) and restarts with the current settings.
	static void ResetToken();

private:
	LocalServer();
	~LocalServer();

	void Accept();
	void Read(QTcpSocket *socket);
	void Handle(QTcpSocket *socket, const QByteArray &method, const QByteArray &path, const QByteArray &body);
	static void Reply(QTcpSocket *socket, int status, const QByteArray &type, const QByteArray &body);

	QTcpServer server;
	QString token;
	bool network = false;
	QList<QTcpSocket *> listeners; // pages with an open event stream
};
