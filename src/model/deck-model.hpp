#pragma once

// MODEL layer: the decks (pages of buttons, their steps and triggers), local-first through LocalStore. Qt Core only.
// Only controllers call the mutators; each one saves through the store before emitting decksChanged.
//
// Stored under the key "decks" (the real store keeps it in decks.json in the plugin's config folder):
//   {"version": 1, "sound_monitoring": true, "decks": [{"id", "name", "columns", "rows", "color",
//    "buttons": [{"id", "text", "color", "text_color", "image", "x", "y", "w", "h", "queueable", "overlappable",
//                 "steps": [...], "release": [...], "triggers": [...], "hotkey": [...]}]}]}
// Step and trigger formats: docs/decks.md.

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QString>

namespace fiji::model {

class LocalStore;

class DeckModel : public QObject {
	Q_OBJECT

public:
	static constexpr const char *kStoreKey = "decks";

	explicit DeckModel(LocalStore &store, QObject *parent = nullptr);

	// Reads the decks from the store (nothing saved yet: no decks).
	void load();

	QJsonArray decks() const { return decks_; }
	bool soundMonitoring() const { return soundMonitoring_; }
	bool hasButton(const QString &id) const;
	// Every trigger of every button, with "button" and "deck" added, for whoever watches for them (the bot).
	QJsonArray triggers() const;

	using Bindings = QHash<QString, QJsonArray>; // per button id: the hotkeys bound in OBS's settings

	// Adds the deck, or replaces the one with the same id. Fills in a missing deck id, and gives a new id to any
	// button whose id is missing or already used by another deck. bindings are kept with the buttons and saved
	// too. Returns the deck as given (ids filled in).
	QJsonObject saveDeck(QJsonObject deck, const Bindings &bindings = {});
	bool deleteDeck(const QString &id, const Bindings &bindings = {});

signals:
	void decksChanged();

private:
	bool save();
	void setBindings(const Bindings &bindings);

	LocalStore &store_;
	QJsonArray decks_;
	bool soundMonitoring_ = true;
};

} // namespace fiji::model
