#include "deck-controller.hpp"

#include "../model/deck-model.hpp"
#include "../obs/deck-ports.hpp"

namespace fiji::controller {

DeckController::DeckController(model::DeckModel &decks, obs::DeckSteps &steps, obs::DeckHotkeys *hotkeys,
			       EventFn events, QObject *parent)
	: QObject(parent),
	  decks_(decks),
	  steps_(steps),
	  hotkeys_(hotkeys),
	  events_(std::move(events)),
	  engine_(this)
{
	clock_.start();
	now_ = [this] { return clock_.elapsed(); };
	ticker_.setInterval(15);
	connect(&ticker_, &QTimer::timeout, this, [this] {
		tick();
		if (engine_.idle()) {
			ticker_.stop();
		}
	});
	connect(&decks_, &model::DeckModel::decksChanged, this, &DeckController::decksChanged);
}

DeckController::~DeckController()
{
	shutdown();
	if (hotkeys_) {
		hotkeys_->clear();
	}
}

void DeckController::start()
{
	decks_.load();
	steps_.setSoundMonitoring(decks_.soundMonitoring());
	engine_.setDecks(decks_.decks());
	if (hotkeys_) {
		hotkeys_->bind(decks_.decks(), [this](const QString &button, bool pressed) {
			pressed ? press(button) : release(button);
		});
	}
}

void DeckController::shutdown()
{
	ticker_.stop();
	engine_.stopAll();
	steps_.stopSounds(QString());
}

void DeckController::decksChanged()
{
	// carry what the streamer bound in OBS's hotkey settings into the decks, so it is saved with them
	if (hotkeys_) {
		decks_.setHotkeyBindings(hotkeys_->bindings());
		decks_.saveNow();
	}
	engine_.setDecks(decks_.decks());
	if (hotkeys_) {
		hotkeys_->bind(decks_.decks(), [this](const QString &button, bool pressed) {
			pressed ? press(button) : release(button);
		});
	}
	if (events_) {
		events_(QStringLiteral("decks_changed"), {});
	}
}

QJsonArray DeckController::decks() const
{
	return decks_.decks();
}

QJsonArray DeckController::triggers() const
{
	return decks_.triggers();
}

QJsonObject DeckController::saveDeck(const QJsonObject &deck)
{
	return decks_.saveDeck(deck);
}

bool DeckController::deleteDeck(const QString &id)
{
	return decks_.deleteDeck(id);
}

void DeckController::wake()
{
	if (!engine_.idle() && !ticker_.isActive()) {
		ticker_.start();
	}
}

bool DeckController::press(const QString &button, const decks::Variables &trigger)
{
	const bool started = engine_.press(button, trigger, now_());
	if (started) {
		tick(); // steps at time zero run right away
		wake();
	}
	return started;
}

bool DeckController::release(const QString &button)
{
	const bool started = engine_.release(button, now_());
	if (started) {
		tick();
		wake();
	}
	return started;
}

void DeckController::stop(const QString &button)
{
	if (button == QStringLiteral("all")) {
		engine_.stopAll();
		steps_.stopSounds(QString());
	} else {
		engine_.stop(button);
	}
	tick(); // reports the stopped buttons as idle
}

void DeckController::execute(const QJsonObject &step)
{
	const QString type = step.value(QStringLiteral("type")).toString();
	const QString text = step.value(QStringLiteral("text")).toString();
	if (type == QStringLiteral("chat")) {
		// only the bot can talk in chat; with no bot linked nothing listens
		if (events_) {
			events_(QStringLiteral("say"), {{QStringLiteral("text"), text}});
		}
	} else if (type == QStringLiteral("notify")) {
		if (events_) {
			events_(QStringLiteral("deck_notification"), {{QStringLiteral("text"), text}});
		}
	} else {
		steps_.run(step);
	}
}

void DeckController::buttonStateChanged(const QString &button, bool running)
{
	if (events_) {
		events_(QStringLiteral("button_state"), {{QStringLiteral("button"), button}, {QStringLiteral("running"), running}});
	}
}

// ---------------------------------------------------------------------- the bridge's deck actions
QStringList DeckController::actions() const
{
	return {"deck.list", "deck.press", "deck.fire", "deck.release", "deck.stop", "deck.save", "deck.delete", "deck.triggers"};
}

CommandResult DeckController::handle(const Command &command)
{
	const auto &a = command.action;
	const auto &p = command.payload;
	const QString button = p.value(QStringLiteral("button")).toString();
	CommandResult r;
	auto fail = [&r](const char *code, const QString &message) {
		r.ok = false;
		r.code = QString::fromLatin1(code);
		r.message = message;
		return r;
	};
	if (a == QStringLiteral("deck.list")) {
		r.result = {{QStringLiteral("decks"), decks()}};
	} else if (a == QStringLiteral("deck.triggers")) {
		r.result = {{QStringLiteral("triggers"), triggers()}};
	} else if (a == QStringLiteral("deck.press") || a == QStringLiteral("deck.fire")) {
		if (button.isEmpty()) {
			return fail("BAD_REQUEST", QStringLiteral("Which button?"));
		}
		if (!press(button, p.value(QStringLiteral("vars")).toObject().toVariantHash())) {
			return fail(engine_.isRunning(button) ? "BUSY" : "NOT_FOUND",
				    engine_.isRunning(button) ? QStringLiteral("That button is still running")
							      : QStringLiteral("There is no button %1").arg(button));
		}
	} else if (a == QStringLiteral("deck.release")) {
		release(button); // a button without release steps simply has nothing to do
	} else if (a == QStringLiteral("deck.stop")) {
		if (button.isEmpty()) {
			return fail("BAD_REQUEST", QStringLiteral("Which button? (\"all\" stops everything)"));
		}
		stop(button);
	} else if (a == QStringLiteral("deck.save")) {
		if (!p.value(QStringLiteral("deck")).isObject()) {
			return fail("BAD_REQUEST", QStringLiteral("A deck is an object"));
		}
		r.result = {{QStringLiteral("deck"), saveDeck(p.value(QStringLiteral("deck")).toObject())}};
	} else if (a == QStringLiteral("deck.delete")) {
		if (!deleteDeck(p.value(QStringLiteral("id")).toString())) {
			return fail("NOT_FOUND", QStringLiteral("There is no such deck"));
		}
	} else {
		return fail("UNKNOWN_ACTION", a);
	}
	return r;
}

} // namespace fiji::controller
