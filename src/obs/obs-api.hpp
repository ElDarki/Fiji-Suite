#pragma once

// The OBS port. Controllers drive OBS only through this interface, so they can be tested with a fake.
// This header stays free of obs.h; only implementations in src/obs/*.cpp include libobs.
// It grows with each controller moved over; today it covers what the bridge actions need.

#include "../model/obs-status.hpp"

#include <QJsonObject>
#include <QString>

namespace fiji::obs {

// The outcome of one call. code is an error code of BRIDGE_CONTRACT.md (NOT_FOUND, BUSY, OBS_ERROR) when !ok.
struct ObsCall {
	bool ok = true;
	QJsonObject data;
	QString code;
	QString message;

	static ObsCall failed(const QString &code, const QString &message) { return {false, {}, code, message}; }
};

class ObsApi {
public:
	virtual ~ObsApi() = default;

	// Everything at once: canvases (the main one first), outputs, decks and running deck buttons.
	virtual model::ObsSnapshot snapshot() = 0;

	// canvas empty: OBS's main canvas.
	virtual ObsCall switchScene(const QString &scene, const QString &canvas) = 0;
	virtual ObsCall setTransition(const QString &canvas, const QString &transition) = 0;
	// Studio mode: put the canvas's preview on program.
	virtual ObsCall runTransition(const QString &canvas) = 0;
	// target: an output name, a group name, stream / record / backtrack / virtual_cam, or all.
	// data: {"started": [names]} or {"stopped": [names]}.
	virtual ObsCall startOutputs(const QString &target) = 0;
	virtual ObsCall stopOutputs(const QString &target) = 0;

	// Decks (the deck format is docs/decks.md). data as named.
	virtual ObsCall decks() = 0;                                              // {"decks": [...]}
	virtual ObsCall pressButton(const QString &button, const QJsonObject &vars) = 0;
	virtual ObsCall releaseButton(const QString &button) = 0;
	virtual ObsCall stopButton(const QString &button) = 0;                    // "all" stops everything
	virtual ObsCall saveDeck(const QJsonObject &deck) = 0;                    // {"deck": {...}}
	virtual ObsCall deleteDeck(const QString &id) = 0;
	virtual ObsCall triggers() = 0;                                           // {"triggers": [...]}
	virtual ObsCall webFile(const QString &name) = 0;                         // {"content", "type"}
	virtual ObsCall look() = 0;                                               // {"dark": bool}
};

} // namespace fiji::obs
