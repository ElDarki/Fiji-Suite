#pragma once

// CONTROLLER layer: once per OBS start (when the user allows it), asks for the newest release and, when it is newer
// than this plugin and not one the user skipped, says so through offerUpdate (the view shows the offer). The user's
// answer comes back through skip().

#include <QObject>
#include <QString>

namespace fiji::obs {
class ReleaseFeed;
class UpdateSettings;
} // namespace fiji::obs

namespace fiji::controller {

class UpdateChecker : public QObject {
	Q_OBJECT

public:
	UpdateChecker(obs::ReleaseFeed &feed, obs::UpdateSettings &settings, QString current, QObject *parent = nullptr);

	void start(); // once per start, however often it is called
	void stop();
	void skip(const QString &version);

	// What the last check found, for the log and tests: "off", "up to date", "skipped", "offered", "failed".
	QString outcome() const { return outcome_; }

signals:
	void offerUpdate(const QString &latest, const QString &current);

private:
	void found(const QString &tag);

	obs::ReleaseFeed &feed_;
	obs::UpdateSettings &settings_;
	QString current_;
	QString outcome_;
	bool started_ = false;
};

} // namespace fiji::controller
