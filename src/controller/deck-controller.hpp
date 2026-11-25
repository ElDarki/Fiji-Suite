#pragma once

// CONTROLLER layer: the decks at work. Runs the deck actions through the dispatcher (the bridge, the local deck
// server, obs-websocket and the hotkeys all post them), drives the deck engine's clock, keeps the key states, and
// hands the steps that act on OBS to obs::DeckSteps. What the outside world should hear about (a key started or
// stopped, a "say in chat" or "notify" step, the decks changed) comes out as signals; the composition turns them into
// the plugin's events. Lives on the UI thread.
//
// Actions: deck.list {} -> {decks}; deck.save {deck} -> {deck}; deck.delete {id}; deck.press / deck.fire {button,
// vars?}; deck.release {button}; deck.stop {button | "all"}; deck.triggers {} -> {triggers}; deck.state {} ->
// {running: [ids], keys: {id: state}}.

#include "../model/deck-engine.hpp"
#include "command-dispatcher.hpp"

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>
#include <functional>

namespace fiji::model {
class DeckModel;
}
namespace fiji::state {
class DeckKeys;
}
namespace fiji::obs {
class DeckSteps;
class DeckHotkeys;
} // namespace fiji::obs

namespace fiji::controller {

class DeckController final : public QObject, public CommandHandler, private decks::Host {
	Q_OBJECT

public:
	DeckController(model::DeckModel &model, state::DeckKeys &keys, obs::DeckSteps &steps, obs::DeckHotkeys &hotkeys,
		       QObject *parent = nullptr);
	~DeckController() override;

	// Loads the decks, hands them to the engine and registers their hotkeys.
	void start();
	// OBS is exiting: stops every key and sound, and lets go of the hotkeys.
	void shutdown();

	QStringList actions() const override;
	CommandResult handle(const Command &command) override;

	// For tests: a hand-driven clock (ms) instead of the real one; the ticker then stays off.
	void setClock(std::function<qint64()> now);
	void tick();

signals:
	void decksChanged();
	void keyChanged(const QString &button, bool running);
	void say(const QString &text);
	void notice(const QString &text);

private:
	void execute(const QJsonObject &step) override;
	void buttonStateChanged(const QString &button, bool running) override;
	void changed();
	void wake();
	qint64 now() const;

	model::DeckModel &model_;
	state::DeckKeys &keys_;
	obs::DeckSteps &steps_;
	obs::DeckHotkeys &hotkeys_;
	decks::Engine engine_;
	QTimer ticker_;
	QElapsedTimer clock_;
	std::function<qint64()> handClock_;
};

} // namespace fiji::controller
