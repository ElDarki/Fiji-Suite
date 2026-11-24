#pragma once

// CONTROLLER layer: the decks at work. Presses, releases and stops (from the deck pages, OBS hotkeys, obs-websocket
// and the bot) run their buttons through the deck engine; the engine's own steps run here, chat and notices become
// events, and every step that acts on OBS goes to the DeckSteps port. Edits go to the DeckModel, which saves them.
//
// Also a dispatcher handler for the bridge's deck actions: deck.list, deck.press, deck.fire, deck.release,
// deck.stop, deck.save, deck.delete, deck.triggers. Lives on the UI thread.

#include "../state/deck-engine.hpp"
#include "command-dispatcher.hpp"

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>
#include <functional>

namespace fiji::model {
class DeckModel;
}
namespace fiji::obs {
class DeckSteps;
class DeckHotkeys;
} // namespace fiji::obs

namespace fiji::controller {

class DeckController final : public QObject, public CommandHandler, public decks::Host {
	Q_OBJECT

public:
	// Events in the plugin's own words: button_state {button, running}, decks_changed, say {text},
	// deck_notification {text}. The composition forwards them to obs-websocket, the deck pages and the bot.
	using EventFn = std::function<void(const QString &name, const QJsonObject &data)>;

	DeckController(model::DeckModel &decks, obs::DeckSteps &steps, obs::DeckHotkeys *hotkeys, EventFn events,
		       QObject *parent = nullptr);
	~DeckController() override;

	// Loads the decks, hands them to the engine and binds their hotkeys.
	void start();
	// OBS is exiting: stop everything and let go of the sounds.
	void shutdown();

	QJsonArray decks() const;
	QJsonArray triggers() const;
	QStringList running() const { return engine_.running(); }
	QJsonObject saveDeck(const QJsonObject &deck);
	bool deleteDeck(const QString &id);
	bool press(const QString &button, const decks::Variables &trigger = {});
	bool release(const QString &button);
	void stop(const QString &button); // "all" stops everything, sounds included

	// For tests: a different clock (ms).
	void setClock(std::function<qint64()> clock) { now_ = std::move(clock); }
	// Runs what is due now (the timer does this every 15 ms while something runs).
	void tick() { engine_.tick(now_()); }

	// CommandHandler
	QStringList actions() const override;
	CommandResult handle(const Command &command) override;

	// decks::Host
	void execute(const QJsonObject &step) override;
	void buttonStateChanged(const QString &button, bool running) override;

private:
	void decksChanged();
	void wake();

	model::DeckModel &decks_;
	obs::DeckSteps &steps_;
	obs::DeckHotkeys *hotkeys_;
	EventFn events_;
	decks::Engine engine_;
	QTimer ticker_;
	QElapsedTimer clock_;
	std::function<qint64()> now_;
};

} // namespace fiji::controller
