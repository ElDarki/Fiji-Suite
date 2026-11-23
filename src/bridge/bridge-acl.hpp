#pragma once

// BRIDGE (the plugin's Anti-Corruption Layer). Translates in both directions and nothing else:
//   COMMAND envelope  -> controller::Command, posted to the CommandDispatcher; its CommandResult -> ACK envelope
//   model/state signals (outputs, scenes, decks) -> EVENT envelopes, only for accepted capabilities
//   HELLO + capability negotiation, SYNC_STATE snapshots (BRIDGE_CONTRACT.md sections 3 and 4)
// Models and controllers never see an envelope; the wire never sees a model. Scaffold: declarations only.

#include "envelope.hpp"

#include <QByteArray>
#include <QObject>
#include <QStringList>
#include <functional>

namespace fiji::controller {
class CommandDispatcher;
}
namespace fiji::state {
class LinkStateMachine;
}

namespace fiji::bridge {

class BridgeAcl : public QObject {
	Q_OBJECT

public:
	BridgeAcl(controller::CommandDispatcher &dispatcher, state::LinkStateMachine &link, QObject *parent = nullptr);

	// What this plugin offers in its HELLO.
	static QStringList offeredCapabilities();

	// The plugin's HELLO, sent first after the socket opens.
	Envelope hello(const QString &appVersion) const;

	// A frame from the transport (link thread). Thread-safe.
	void onFrame(const QByteArray &frame);

	// The transport's send function (set by the transport; called from any thread).
	std::function<void(const QByteArray &frame)> send;

	// Publishing (UI thread): no-ops unless LINKED and the capability was accepted.
	void publishSnapshot();
	void publishEvent(const QString &action, const QJsonObject &payload);
};

} // namespace fiji::bridge
