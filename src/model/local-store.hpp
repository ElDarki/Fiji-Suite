#pragma once

// MODEL layer (CLAUDE.md, "MVC + State"): Qt Core only. No obs.h, no widgets, no bridge.
//
// Local-first persistence. Every model saves through a LocalStore, so the plugin keeps working with no bot at all.
// The real store is ObsDataStore (src/obs/), which writes obs_data_t JSON into the plugin's config folder; tests use
// an in-memory store.

#include <QJsonObject>
#include <QString>

namespace fiji::model {

class LocalStore {
public:
	virtual ~LocalStore() = default;

	// The object saved under `key` (e.g. "decks", "settings", "canvases"); empty when nothing was saved yet.
	virtual QJsonObject load(const QString &key) const = 0;

	// Saves atomically (write to a temp file, then rename). False when the disk refused.
	virtual bool save(const QString &key, const QJsonObject &value) = 0;
};

} // namespace fiji::model
