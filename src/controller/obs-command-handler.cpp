#include "obs-command-handler.hpp"

#include "../obs/obs-api.hpp"

namespace fiji::controller {

namespace {

CommandResult from(const obs::ObsCall &call)
{
	CommandResult r;
	r.ok = call.ok;
	r.result = call.data;
	r.code = call.ok ? QString() : (call.code.isEmpty() ? QStringLiteral("OBS_ERROR") : call.code);
	r.message = call.message;
	r.retryable = r.code == QStringLiteral("BUSY");
	return r;
}

CommandResult badRequest(const QString &message)
{
	CommandResult r;
	r.ok = false;
	r.code = QStringLiteral("BAD_REQUEST");
	r.message = message;
	return r;
}

} // namespace

QStringList ObsCommandHandler::actions() const
{
	return {"scene.switch", "transition.set", "transition.run", "output.start", "output.stop", "deck.list",
		"deck.press",   "deck.fire",      "deck.release",   "deck.stop",    "deck.save",   "deck.delete",
		"deck.file",    "look.get",       "deck.triggers",  "state.snapshot"};
}

CommandResult ObsCommandHandler::handle(const Command &command)
{
	const auto &p = command.payload;
	const auto text = [&p](const char *key) { return p.value(QLatin1String(key)).toString(); };
	const auto &a = command.action;

	if (a == "state.snapshot") {
		CommandResult r;
		r.result = obs_.snapshot().toJson();
		return r;
	}
	if (a == "scene.switch") {
		if (text("scene").isEmpty()) {
			return badRequest(QStringLiteral("Which scene?"));
		}
		return from(obs_.switchScene(text("scene"), text("canvas")));
	}
	if (a == "transition.set") {
		return from(obs_.setTransition(text("canvas"), text("transition")));
	}
	if (a == "transition.run") {
		return from(obs_.runTransition(text("canvas")));
	}
	if (a == "output.start" || a == "output.stop") {
		if (text("target").isEmpty()) {
			return badRequest(QStringLiteral("Which output?"));
		}
		return from(a == "output.start" ? obs_.startOutputs(text("target")) : obs_.stopOutputs(text("target")));
	}
	if (a == "deck.list") {
		return from(obs_.decks());
	}
	if (a == "deck.press" || a == "deck.fire") {
		if (text("button").isEmpty()) {
			return badRequest(QStringLiteral("Which button?"));
		}
		return from(obs_.pressButton(text("button"), p.value("vars").toObject()));
	}
	if (a == "deck.release") {
		return from(obs_.releaseButton(text("button")));
	}
	if (a == "deck.stop") {
		return from(obs_.stopButton(text("button")));
	}
	if (a == "deck.save") {
		if (!p.value("deck").isObject()) {
			return badRequest(QStringLiteral("A deck is an object"));
		}
		return from(obs_.saveDeck(p.value("deck").toObject()));
	}
	if (a == "deck.delete") {
		return from(obs_.deleteDeck(text("id")));
	}
	if (a == "deck.file") {
		return from(obs_.webFile(text("name")));
	}
	if (a == "look.get") {
		return from(obs_.look());
	}
	if (a == "deck.triggers") {
		return from(obs_.triggers());
	}
	CommandResult r;
	r.ok = false;
	r.code = QStringLiteral("UNKNOWN_ACTION");
	r.message = a;
	return r;
}

} // namespace fiji::controller
