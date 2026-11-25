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
	return {"scene.switch", "transition.set", "transition.run", "deck.file", "look.get", "state.snapshot"};
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
	if (a == "deck.file") {
		return from(obs_.webFile(text("name")));
	}
	if (a == "look.get") {
		return from(obs_.look());
	}
	CommandResult r;
	r.ok = false;
	r.code = QStringLiteral("UNKNOWN_ACTION");
	r.message = a;
	return r;
}

} // namespace fiji::controller
