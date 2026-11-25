#pragma once

// OBS adapter for the Capture dock: follows OBS's game and window captures (created, removed, renamed, hooked,
// unhooked) into model::Captures on the UI thread, and is the CapturePort for choosing a capture's window.

#include "capture-port.hpp"

#include <QHash>
#include <QObject>

struct obs_source;
struct obs_weak_source;
struct calldata;

namespace fiji::model {
class Captures;
}

namespace fiji::obs {

class ObsCaptures final : public QObject, public CapturePort {
	Q_OBJECT

public:
	explicit ObsCaptures(model::Captures &captures, QObject *parent = nullptr);
	~ObsCaptures() override;

	QList<WindowChoice> windows(const QString &captureKey) override;
	void chooseWindow(const QString &captureKey, const QVariant &value) override;

private:
	struct Watched;
	void watch(obs_source *source);
	void unwatch(const QString &key);
	obs_source *sourceOf(const QString &key) const; // a new reference, or null

	static void source_create(void *data, calldata *cd);
	static void source_remove(void *data, calldata *cd);
	static void hooked(void *data, calldata *cd);
	static void unhooked(void *data, calldata *cd);
	static void rename(void *data, calldata *cd);

	model::Captures &captures_;
	QHash<QString, Watched *> watched_; // UI thread: by source uuid
};

} // namespace fiji::obs
