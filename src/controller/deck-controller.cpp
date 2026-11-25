#include "deck-controller.hpp"

#include "../model/deck-model.hpp"
#include "../obs/deck-port.hpp"
#include "../state/deck-keys.hpp"

#include <QJsonArray>

namespace fiji::controller {

namespace {

CommandResult failed(const char *code, const QString &message)
{
	CommandResult r;
	r.ok = false;
	r.code = QString::fromLatin1(code);
	r.message = message;
	r.retryable = r.code == QStringLiteral("BUSY");
	return r;
}

CommandResult ok(const QJsonObject &result = {})
{
	CommandResult r;
	r.result = result;
	return r;
}

} // namespace

DeckController::DeckController(model::DeckModel &model, state::DeckKeys &keys, obs::DeckSteps &steps,
			       obs::DeckHotkeys &hotkeys, QObject *parent)
	: QObject(parent),
	  model_(model),
	  keys_(keys),
	  steps_(steps),
	  hotkeys_(hotkeys),
	  engine_(this)
{
	clock_.start();
	ticker_.setInterval(15);
	connect(&ticker_, &QTimer::timeout, this, [this] {
		tick();
		if (engine_.idle()) {
			ticker_.stop();
		}
	});
}

DeckController::~DeckController() = default;

void DeckController::start()
{
	model_.load();
	steps_.setSoundMonitoring(model_.soundMonitoring());
	engine_.setDecks(model_.decks());
	hotkeys_.registerAll(model_.decks());
}

void DeckController::shutdown()
{
	ticker_.stop();
	engine_.stopAll();
	steps_.stopSounds(QString());
	keys_.clear();
	hotkeys_.unregisterAll();
}

void DeckController::setClock(std::function<qint64()> now)
{
	handClock_ = std::move(now);
}

qint64 DeckController::now() const
{
	return handClock_ ? handClock_() : clock_.elapsed();
}

void DeckController::tick()
{
	engine_.tick(now());
}

void DeckController::wake()
{
	if (!engine_.idle() && !handClock_ && !ticker_.isActive()) {
		ticker_.start();
	}
}

// Edited decks: keep what was bound in OBS, give the engine the new buttons, register their hotkeys again.
void DeckController::changed()
{
	engine_.setDecks(model_.decks());
	hotkeys_.registerAll(model_.decks());
	emit decksChanged();
}

QStringList DeckController::actions() const
{
	return {"deck.list",    "deck.save", "deck.delete",   "deck.press", "deck.fire",
		"deck.release", "deck.stop", "deck.triggers", "deck.state"};
}

CommandResult DeckController::handle(const Command &command)
{
	const auto &a = command.action;
	const auto &p = command.payload;
	const QString button = p.value(QStringLiteral("button")).toString();

	if (a == QStringLiteral("deck.list")) {
		return ok({{QStringLiteral("decks"), model_.decks()}});
	}
	if (a == QStringLiteral("deck.triggers")) {
		return ok({{QStringLiteral("triggers"), model_.triggers()}});
	}
	if (a == QStringLiteral("deck.state")) {
		QJsonObject keys;
		for (const auto &deck : model_.decks()) {
			for (const auto &b : deck.toObject().value(QStringLiteral("buttons")).toArray()) {
				const QString id = b.toObject().value(QStringLiteral("id")).toString();
				if (keys_.state(id) != state::KeyState::IDLE) {
					keys.insert(id, state::keyStateName(keys_.state(id)));
				}
			}
		}
		return ok({{QStringLiteral("running"), QJsonArray::fromStringList(engine_.running())},
			   {QStringLiteral("keys"), keys}});
	}
	if (a == QStringLiteral("deck.save") || a == QStringLiteral("deck.delete")) {
		const auto bindings = hotkeys_.bindings(); // what was bound in OBS, kept before the hotkeys go
		hotkeys_.unregisterAll();
		CommandResult result;
		if (a == QStringLiteral("deck.save")) {
			if (!p.value(QStringLiteral("deck")).isObject()) {
				hotkeys_.registerAll(model_.decks());
				return failed("BAD_REQUEST", QStringLiteral("'deck' not set"));
			}
			result = ok({{QStringLiteral("deck"), model_.saveDeck(p.value(QStringLiteral("deck")).toObject(), bindings)}});
		} else if (!model_.deleteDeck(p.value(QStringLiteral("id")).toString(), bindings)) {
			hotkeys_.registerAll(model_.decks());
			return failed("NOT_FOUND", QStringLiteral("'id' not found"));
		} else {
			result = ok();
		}
		changed();
		return result;
	}
	if (a == QStringLiteral("deck.press") || a == QStringLiteral("deck.fire")) {
		decks::Variables trigger;
		const auto vars = p.value(QStringLiteral("vars")).toObject();
		for (auto it = vars.constBegin(); it != vars.constEnd(); ++it) {
			trigger.insert(it.key(), it.value().toVariant());
		}
		if (command.source == CommandSource::Hotkey) {
			keys_.input(button, state::KeyInput::Pressed);
		}
		if (!engine_.press(button, trigger, now())) {
			return model_.hasButton(button) ? failed("BUSY", QStringLiteral("That key is already running"))
							: failed("NOT_FOUND", QStringLiteral("no such button"));
		}
		tick(); // steps at time zero run right away
		wake();
		return ok();
	}
	if (a == QStringLiteral("deck.release")) {
		if (command.source == CommandSource::Hotkey) {
			keys_.input(button, state::KeyInput::Released);
		}
		if (engine_.release(button, now())) {
			tick();
			wake();
		}
		return ok();
	}
	if (a == QStringLiteral("deck.stop")) {
		if (button.isEmpty()) {
			return failed("BAD_REQUEST", QStringLiteral("'button' not set (use \"all\" to stop everything)"));
		}
		if (button == QStringLiteral("all")) {
			engine_.stopAll();
			steps_.stopSounds(QString());
		} else {
			engine_.stop(button);
		}
		tick(); // reports the stopped buttons as idle
		return ok();
	}
	return failed("UNKNOWN_ACTION", a);
}

void DeckController::execute(const QJsonObject &step)
{
	const QString type = step.value(QStringLiteral("type")).toString();
	if (type == QStringLiteral("chat")) {
		emit say(step.value(QStringLiteral("text")).toString()); // only the bot can talk in chat
	} else if (type == QStringLiteral("notify")) {
		emit notice(step.value(QStringLiteral("text")).toString());
	} else {
		steps_.run(step);
	}
}

void DeckController::buttonStateChanged(const QString &button, bool running)
{
	keys_.input(button, running ? state::KeyInput::RunStarted : state::KeyInput::RunEnded);
	emit keyChanged(button, running);
}

} // namespace fiji::controller
