#pragma once

// The OBS ports the deck controller needs. Free of obs.h; the libobs implementations are obs-deck-steps.cpp and
// obs-deck-hotkeys.cpp, and tests use fakes.

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <functional>

namespace fiji::obs {

// The deck steps that act on OBS (docs/decks.md): scene, source_visibility, source_position, source_scale,
// source_rotation, filter_visibility, source_text, mute, sound, stop_sounds, request. Variables are already filled in.
class DeckSteps {
public:
	virtual ~DeckSteps() = default;
	// False (and a line in the OBS log) when the step could not be carried out, or is not one of these.
	virtual bool run(const QJsonObject &step) = 0;
	// Stops the deck sounds with that name; an empty name stops them all.
	virtual void stopSounds(const QString &name) = 0;
	// Whether deck sounds are also heard on the streamer's monitoring device.
	virtual void setSoundMonitoring(bool on) = 0;
};

// One OBS hotkey per deck button (Settings -> Hotkeys).
class DeckHotkeys {
public:
	using KeyFn = std::function<void(const QString &button, bool pressed)>;

	virtual ~DeckHotkeys() = default;
	// Registers a hotkey for every button of the decks, with its saved bindings ("hotkey"). onKey is called on the
	// UI thread.
	virtual void bind(const QJsonArray &decks, KeyFn onKey) = 0;
	// What the streamer bound in OBS, per button id.
	virtual QHash<QString, QJsonArray> bindings() const = 0;
	virtual void clear() = 0;
};

} // namespace fiji::obs
