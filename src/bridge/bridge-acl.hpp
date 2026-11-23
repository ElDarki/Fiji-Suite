#pragma once

// BRIDGE (the plugin's Anti-Corruption Layer). Translates in both directions and nothing else:
//   the bot's HELLO           -> the accepted capabilities, LinkStateMachine::helloCompleted (or a version refusal)
//   COMMAND envelope          -> controller::Command posted to the CommandDispatcher; its CommandResult -> ACK
//   the plugin's own events   -> EVENT envelopes, only for accepted capabilities
//   model::ObsSnapshot        -> SYNC_STATE (after the handshake, on state.sync, and when no event covers a change)
// Models and controllers never see an envelope; the wire never sees a model. Only fields the catalogue defines are
// read or written (catalogue.hpp, a copy of BRIDGE_CONTRACT.md section 6).
//
// Threads: the transport calls onOpen/onFrame/onClosed on its own thread; they are thread-safe. Everything that
// touches the state machine or OBS is queued to this object's (the UI) thread.

#include "envelope.hpp"

#include <QByteArray>
#include <QHash>
#include <QJsonObject>
#include <QMutex>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <atomic>
#include <functional>

namespace fiji::controller {
class CommandDispatcher;
struct CommandResult;
} // namespace fiji::controller
namespace fiji::model {
struct ObsSnapshot;
}
namespace fiji::state {
class LinkStateMachine;
}

namespace fiji::bridge {

class BridgeAcl : public QObject {
	Q_OBJECT

public:
	BridgeAcl(controller::CommandDispatcher &dispatcher, state::LinkStateMachine &link, QString appVersion,
		  QObject *parent = nullptr);

	// What this plugin offers in its HELLO.
	static QStringList offeredCapabilities();
	// What the main canvas is called on the wire.
	static QString mainCanvasName();

	// ---- the transport's side (any thread) ----
	std::function<void(const QByteArray &frame)> send; // set by the transport; thread-safe
	std::function<void()> refuse;                      // the bot speaks another major version: stop retrying
	void onOpen();                                     // sends the HELLO
	void onFrame(const QByteArray &frame);
	void onClosed();
	bool linked() const;                               // the handshake is done on the current connection

	// ---- the plugin's side (UI thread) ----
	// An event in the plugin's own words (the names fiji_emit_event uses, e.g. "button_state", "stop_output").
	void onPluginEvent(const QString &name, const QJsonObject &data);
	// Sends a SYNC_STATE; replyTo is the state.sync COMMAND's id, or empty for a fresh one.
	void publishSnapshot(const QString &replyTo = QString());
	// Sends output.dropped for every active output (about every 15 s, from a timer).
	void reportDroppedFrames();

private:
	void handleHello(const Envelope &hello);
	void handleCommand(const Envelope &command);
	void sendEnvelope(const Envelope &envelope);
	void sendEvent(const QString &action, const QJsonObject &payload);
	void sendAck(const QString &id, const QString &action, const controller::CommandResult &result);
	bool accepts(const QString &capability) const;
	void withSnapshot(std::function<void(const model::ObsSnapshot &)> then);
	QJsonObject wireSnapshot(const model::ObsSnapshot &snapshot);

	controller::CommandDispatcher &dispatcher_;
	state::LinkStateMachine &link_;
	QString appVersion_;
	mutable QMutex mutex_; // guards accepted_ and helloId_
	QStringList accepted_;
	QString helloId_;
	std::atomic<bool> linked_{false};
	QHash<QString, QString> outputKinds_; // UI thread: output name -> kind, from the last snapshot
	QTimer droppedTimer_;
};

} // namespace fiji::bridge
