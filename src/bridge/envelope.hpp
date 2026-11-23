#pragma once

// BRIDGE (the plugin's Anti-Corruption Layer): the envelope of BRIDGE_CONTRACT.md section 2. Only src/bridge/ may
// use this. kContractVersion must equal the contract's version (tools/check-architecture.py checks it).
// Scaffold: declarations only.

#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <optional>

namespace fiji::bridge {

inline constexpr const char *kContractVersion = "1.1";
inline constexpr int kAckTimeoutMs = 10000;

enum class MessageType { COMMAND, EVENT, ACK, HELLO, SYNC_STATE };

struct Envelope {
	QString version = QString::fromLatin1(kContractVersion);
	QString id; // empty for EVENT
	MessageType type = MessageType::EVENT;
	QString action;
	QJsonObject payload;

	// One text frame -> an envelope, or nothing when the frame breaks section 2's rules (dropped, logged once).
	static std::optional<Envelope> parse(const QByteArray &frame);
	QByteArray serialise() const;
};

} // namespace fiji::bridge
