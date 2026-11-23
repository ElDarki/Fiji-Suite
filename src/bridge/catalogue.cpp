#include "catalogue.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace fiji::bridge {

namespace {

// The block of BRIDGE_CONTRACT.md section 6, byte for byte (tools/check-architecture.py compares them). Change the
// contract first, then paste its block here.
const char *const kCatalogueJson = R"json(
{
  "version": "1.2",
  "types": ["COMMAND", "EVENT", "ACK", "HELLO", "SYNC_STATE"],
  "errors": ["BAD_REQUEST", "UNKNOWN_ACTION", "NOT_FOUND", "BUSY", "OBS_ERROR", "TIMEOUT", "NOT_LINKED"],
  "output_states": ["IDLE", "STARTING", "ACTIVE", "RECONNECTING", "STOPPING", "FAILED"],
  "capabilities": ["scenes", "transitions", "outputs", "output.alerts", "decks", "decks.pages", "chat", "triggers"],
  "actions": {
    "link.hello":     {"type": "HELLO", "dir": "both", "capability": null,
                       "payload": {"required": ["role", "app", "app_version", "capabilities"],
                                   "optional": ["accepted", "reject"]}},
    "state.sync":     {"type": "COMMAND", "dir": "both", "capability": null,
                       "payload": {"required": [], "optional": []}, "result": []},
    "state.snapshot": {"type": "SYNC_STATE", "dir": "P>B", "capability": null,
                       "payload": {"required": [], "optional": ["canvases", "outputs", "decks", "running"]}},
    "scene.switch":   {"type": "COMMAND", "dir": "B>P", "capability": "scenes",
                       "payload": {"required": ["scene"], "optional": ["canvas"]}, "result": []},
    "transition.set": {"type": "COMMAND", "dir": "B>P", "capability": "transitions",
                       "payload": {"required": ["canvas", "transition"], "optional": []}, "result": []},
    "transition.run": {"type": "COMMAND", "dir": "B>P", "capability": "transitions",
                       "payload": {"required": ["canvas"], "optional": []}, "result": []},
    "scene.program":  {"type": "EVENT", "dir": "P>B", "capability": "scenes",
                       "payload": {"required": ["canvas", "scene"], "optional": ["previous"]}},
    "scene.list":     {"type": "EVENT", "dir": "P>B", "capability": "scenes",
                       "payload": {"required": ["canvas", "scenes"], "optional": []}},
    "output.start":   {"type": "COMMAND", "dir": "B>P", "capability": "outputs",
                       "payload": {"required": ["target"], "optional": []}, "result": ["started"]},
    "output.stop":    {"type": "COMMAND", "dir": "B>P", "capability": "outputs",
                       "payload": {"required": ["target"], "optional": []}, "result": ["stopped"]},
    "output.state":   {"type": "EVENT", "dir": "P>B", "capability": "outputs",
                       "payload": {"required": ["output", "kind", "state"], "optional": ["error"]}},
    "output.dropped": {"type": "EVENT", "dir": "P>B", "capability": "output.alerts",
                       "payload": {"required": ["output", "dropped_frames", "total_frames"], "optional": []}},
    "deck.list":      {"type": "COMMAND", "dir": "B>P", "capability": "decks",
                       "payload": {"required": [], "optional": []}, "result": ["decks"]},
    "deck.press":     {"type": "COMMAND", "dir": "B>P", "capability": "decks",
                       "payload": {"required": ["button"], "optional": ["vars"]}, "result": []},
    "deck.release":   {"type": "COMMAND", "dir": "B>P", "capability": "decks",
                       "payload": {"required": ["button"], "optional": []}, "result": []},
    "deck.stop":      {"type": "COMMAND", "dir": "B>P", "capability": "decks",
                       "payload": {"required": ["button"], "optional": []}, "result": []},
    "deck.save":      {"type": "COMMAND", "dir": "B>P", "capability": "decks",
                       "payload": {"required": ["deck"], "optional": []}, "result": ["deck"]},
    "deck.delete":    {"type": "COMMAND", "dir": "B>P", "capability": "decks",
                       "payload": {"required": ["id"], "optional": []}, "result": []},
    "deck.file":      {"type": "COMMAND", "dir": "B>P", "capability": "decks.pages",
                       "payload": {"required": ["name"], "optional": []}, "result": ["content", "type"]},
    "look.get":       {"type": "COMMAND", "dir": "B>P", "capability": "decks.pages",
                       "payload": {"required": [], "optional": []}, "result": ["dark"]},
    "deck.triggers":  {"type": "COMMAND", "dir": "B>P", "capability": "triggers",
                       "payload": {"required": [], "optional": []}, "result": ["triggers"]},
    "deck.fire":      {"type": "COMMAND", "dir": "B>P", "capability": "triggers",
                       "payload": {"required": ["button"], "optional": ["vars"]}, "result": []},
    "deck.changed":   {"type": "EVENT", "dir": "P>B", "capability": "decks",
                       "payload": {"required": [], "optional": []}},
    "deck.key":       {"type": "EVENT", "dir": "P>B", "capability": "decks",
                       "payload": {"required": ["button", "running"], "optional": []}},
    "deck.notice":    {"type": "EVENT", "dir": "P>B", "capability": "decks",
                       "payload": {"required": ["text"], "optional": []}},
    "chat.say":       {"type": "EVENT", "dir": "P>B", "capability": "chat",
                       "payload": {"required": ["text"], "optional": []}}
  }
}
)json";

QStringList names(const QJsonValue &value)
{
	QStringList list;
	for (const auto &v : value.toArray()) {
		list.append(v.toString());
	}
	return list;
}

Catalogue load()
{
	Catalogue c;
	const auto root = QJsonDocument::fromJson(QByteArray(kCatalogueJson)).object();
	c.version = root.value("version").toString();
	c.capabilities = names(root.value("capabilities"));
	c.errors = names(root.value("errors"));
	const auto actions = root.value("actions").toObject();
	for (auto it = actions.begin(); it != actions.end(); ++it) {
		const auto spec = it.value().toObject();
		const auto payload = spec.value("payload").toObject();
		c.actions.insert(it.key(), ActionSpec{spec.value("type").toString(), spec.value("dir").toString(),
						      spec.value("capability").toString(), names(payload.value("required")),
						      names(payload.value("optional")), names(spec.value("result"))});
	}
	return c;
}

} // namespace

const Catalogue &catalogue()
{
	static const Catalogue c = load(); // thread-safe initialisation
	return c;
}

} // namespace fiji::bridge
