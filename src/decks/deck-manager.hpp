#pragma once

#include "engine-core.hpp"
#include <obs.hpp>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QTimer>
#include <vector>

// Owns the decks: keeps them in decks.json in the plugin's config folder, runs their buttons
// through the engine, and carries out the steps that act on OBS. Lives on the UI thread.
//
// decks.json: {"version": 1, "sound_monitoring": true, "decks": [{"id", "name", "columns", "rows",
//   "color", "buttons": [{"id", "text", "color", "text_color", "image", "x", "y", "w", "h",
//   "queueable", "overlappable", "steps": [...], "release": [...], "triggers": [...], "hotkey": [...]}]}]}
// Steps the engine does not handle itself are documented in docs/decks.md.
class DeckManager : public QObject, public decks::Host {
	Q_OBJECT
public:
	static void Create();
	static void Destroy();
	static DeckManager *Get();

	QJsonArray Decks() const { return decks; }
	// Adds the deck, or replaces the one with the same id. Fills in missing ids. Returns it as stored.
	QJsonObject SaveDeck(QJsonObject deck);
	bool DeleteDeck(const QString &id);

	bool Press(const QString &button, const decks::Variables &trigger);
	bool Release(const QString &button);
	void Stop(const QString &button); // "all" stops everything, sounds included
	QStringList Running() const { return engine.running(); }
	// Every trigger of every button, with "button" and "deck" added, for whoever watches for them.
	QJsonArray Triggers() const;

	// OBS is exiting: stop everything and let go of the sound sources.
	void Shutdown();

	void execute(const QJsonObject &step) override;
	void buttonStateChanged(const QString &button, bool running) override;

private:
	struct Sound {
		int channel;
		OBSSource source;
		QString name;
	};
	struct Hotkey {
		QString button;
		obs_hotkey_id id;
	};

	DeckManager();
	~DeckManager();

	void Load();
	void Save();
	void Changed(); // decks were edited: tell the engine, re-register hotkeys, save, notify
	void RegisterHotkeys();
	void UnregisterHotkeys(bool keepBindings);
	void Wake();

	void PlaySound(const QJsonObject &step);
	void StopSounds(const QString &name);
	static void SoundEnded(void *data, calldata_t *calldata);

	QJsonArray decks;
	bool soundMonitoring = true;
	decks::Engine engine;
	QTimer ticker;
	QElapsedTimer clock;
	std::vector<Sound> sounds;
	std::vector<Hotkey> hotkeys;
};
