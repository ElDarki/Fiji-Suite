#pragma once

// MODEL layer: the decks (pages of buttons, their steps, triggers and hotkey bindings), local-first through
// LocalStore under the key "decks" (decks.json in the plugin's config folder). Qt Core only.
//
// Stored as {"version": 1, "sound_monitoring": true, "decks": [{"id", "name", "columns", "rows", "color",
//   "buttons": [{"id", "text", "color", "text_color", "image", "x", "y", "w", "h", "queueable", "overlappable",
//   "steps": [...], "release": [...], "triggers": [...], "hotkey": [...]}]}]}
// The step types are documented in docs/decks.md.

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <functional>

namespace fiji::model {

class LocalStore;

class DeckModel : public QObject {
	Q_OBJECT

public:
	static constexpr const char *kStoreKey = "decks";

	explicit DeckModel(LocalStore &store, QObject *parent = nullptr);

	// Reads the saved decks (none when nothing was saved yet).
	void load();

	QJsonArray decks() const { return decks_; }
	bool soundMonitoring() const { return soundMonitoring_; }
	// Every trigger of every button, with "button" and "deck" added, for whoever watches for them.
	QJsonArray triggers() const;

	// Adds the deck, or replaces the one with the same id. Fills in a missing deck id, and gives a new id to any
	// button whose id is missing or already used by another deck (button ids are unique across all decks: presses,
	// triggers and other buttons refer to them). Saves, emits decksChanged, and returns the deck as stored.
	QJsonObject saveDeck(QJsonObject deck);
	bool deleteDeck(const QString &id);

	// The hotkey bindings OBS holds for each button id; kept in the decks so they are saved with them. Does not save
	// or emit by itself (saveNow does).
	void setHotkeyBindings(const QHash<QString, QJsonArray> &bindings);
	bool saveNow();

	// For tests: a different id source.
	void setIdSource(std::function<QString()> source) { newId_ = std::move(source); }

signals:
	void decksChanged();

private:
	LocalStore &store_;
	QJsonArray decks_;
	bool soundMonitoring_ = true;
	std::function<QString()> newId_;
};

} // namespace fiji::model
