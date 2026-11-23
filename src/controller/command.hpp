#pragma once

// CONTROLLER layer (CLAUDE.md, "MVC + State"): one typed request to change OBS or a model, whoever asked for it.
// The bridge ACL builds these from COMMAND envelopes; hotkeys, docks and the local deck server build them directly.

#include <QJsonObject>
#include <QString>
#include <functional>

namespace fiji::controller {

enum class CommandSource { Bridge, Hotkey, Ui, LocalServer, WebSocket };

struct Command {
	QString action;        // a catalogue action name, e.g. "scene.switch" (BRIDGE_CONTRACT.md) or a local one
	QJsonObject payload;   // already translated and validated by whoever built the command
	CommandSource source = CommandSource::Ui;
	QString replyTo;       // the envelope id to ACK, for bridge commands
};

struct CommandResult {
	bool ok = true;
	QJsonObject result;
	QString code;          // an error code from BRIDGE_CONTRACT.md when !ok
	QString message;
	bool retryable = false;
};

// Always called on the dispatcher's consumer (OBS/UI) thread, not on the thread that posted the command.
// Senders that live on another thread hop back themselves (the bridge queues the ACK on its transport).
using ReplyFn = std::function<void(const CommandResult &)>;

} // namespace fiji::controller
