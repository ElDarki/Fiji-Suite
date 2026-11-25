#pragma once

// The OBS side of the decks, as the deck controller needs it. Free of obs.h; the implementations (obs-deck-steps.cpp,
// obs-deck-hotkeys.cpp) are the only deck code that calls libobs.

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <functional>

namespace fiji::obs {

// Carries out the deck steps that act on OBS (docs/decks.md): scene, source_visibility, source_position,
// source_scale, source_rotation, filter_visibility, source_text, mute, sound, stop_sounds, request. A step that
// cannot be done is logged and skipped; a deck keeps running.
class DeckSteps {
public:
	virtual ~DeckSteps() = default;
	virtual void run(const QJsonObject &step) = 0;
	virtual void stopSounds(const QString &name) = 0; // empty: every sound
	virtual void setSoundMonitoring(bool on) = 0;     // whether the streamer hears deck sounds too
};

// One OBS hotkey per deck button (Settings -> Hotkeys), bound by the streamer in OBS.
class DeckHotkeys {
public:
	virtual ~DeckHotkeys() = default;
	virtual void registerAll(const QJsonArray &decks) = 0;
	virtual void unregisterAll() = 0;
	// What the streamer bound, per button id, so it is saved with the decks.
	virtual QHash<QString, QJsonArray> bindings() const = 0;

	// Set by the composition; called on the UI thread when a hotkey goes down (true) or up (false).
	std::function<void(const QString &button, bool pressed)> onKey;
};

} // namespace fiji::obs
