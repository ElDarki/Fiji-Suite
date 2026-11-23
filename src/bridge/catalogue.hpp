#pragma once

// BRIDGE: the contract's machine-readable catalogue (BRIDGE_CONTRACT.md section 6), as the ACL reads it.

#include <QHash>
#include <QString>
#include <QStringList>

namespace fiji::bridge {

struct ActionSpec {
	QString type;       // COMMAND, EVENT, HELLO, SYNC_STATE
	QString dir;        // B>P, P>B, both
	QString capability; // empty: always allowed
	QStringList required;
	QStringList optional;
	QStringList result; // the ACK result fields of a COMMAND

	bool defines(const QString &field) const { return required.contains(field) || optional.contains(field); }
};

struct Catalogue {
	QString version;
	QStringList capabilities;
	QStringList errors;
	QHash<QString, ActionSpec> actions;
};

const Catalogue &catalogue();

} // namespace fiji::bridge
