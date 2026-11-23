#include "envelope.hpp"

#include <QJsonDocument>
#include <QUuid>

namespace fiji::bridge {

namespace {
const char *const kTypes[] = {"COMMAND", "EVENT", "ACK", "HELLO", "SYNC_STATE"};
}

QString typeName(MessageType type)
{
	return QString::fromLatin1(kTypes[int(type)]);
}

QString majorOf(const QString &version)
{
	return version.section(QLatin1Char('.'), 0, 0);
}

QString newId()
{
	return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

std::optional<Envelope> Envelope::parse(const QByteArray &frame)
{
	QJsonParseError error;
	const auto doc = QJsonDocument::fromJson(frame, &error);
	if (error.error != QJsonParseError::NoError || !doc.isObject()) {
		return std::nullopt;
	}
	const auto o = doc.object();
	if (!o.value("version").isString() || !o.value("action").isString() || o.value("action").toString().isEmpty()) {
		return std::nullopt;
	}
	const auto kind = o.value("type").toString();
	Envelope e;
	bool known = false;
	for (int i = 0; i < 5; i++) {
		if (kind == QLatin1String(kTypes[i])) {
			e.type = MessageType(i);
			known = true;
		}
	}
	if (!known) {
		return std::nullopt;
	}
	const auto payload = o.value("payload");
	if (!payload.isUndefined() && !payload.isNull() && !payload.isObject()) {
		return std::nullopt;
	}
	e.version = o.value("version").toString();
	e.id = o.value("id").toString(); // null or missing: empty
	e.action = o.value("action").toString();
	e.payload = payload.toObject();
	return e;
}

QByteArray Envelope::serialise() const
{
	QJsonObject o{{"version", version},
		      {"id", id.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(id)},
		      {"type", typeName(type)},
		      {"action", action},
		      {"payload", payload}};
	return QJsonDocument(o).toJson(QJsonDocument::Compact);
}

} // namespace fiji::bridge
