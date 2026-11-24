#pragma once

// CONTROLLER: the capture dock's sources. Brings the port's reports (any thread) onto this object's thread as signals,
// keeps the current list for a view that starts later, and passes window picks to OBS. Qt Core only.

#include "../obs/capture-ports.hpp"

#include <QList>
#include <QObject>
#include <QString>

namespace fiji::controller {

struct CaptureEntry {
	QString uuid;
	QString name;
	QString capture; // what it captures now, as shown ("[game.exe]: Title"); empty when nothing
};

// "[executable]: title", either part left out when empty.
QString captureText(const QString &executable, const QString &title);

class CaptureController : public QObject {
	Q_OBJECT

public:
	explicit CaptureController(obs::CaptureSources &sources, QObject *parent = nullptr);

	// Starts listening to the port (once).
	void start();
	QList<CaptureEntry> entries() const { return entries_; }
	QList<obs::CaptureWindow> windows(const QString &uuid) { return sources_.windows(uuid); }
	void pickWindow(const QString &uuid, const QVariant &value) { sources_.pickWindow(uuid, value); }

signals:
	void added(const fiji::controller::CaptureEntry &entry);
	void removed(const QString &uuid);
	void changed(const fiji::controller::CaptureEntry &entry); // renamed, or captures something else

private:
	void onAdded(const QString &uuid, const QString &name);
	void onRemoved(const QString &uuid);
	void onRenamed(const QString &uuid, const QString &name);
	void onHooked(const QString &uuid, const QString &capture);
	int indexOf(const QString &uuid) const;

	obs::CaptureSources &sources_;
	QList<CaptureEntry> entries_;
	bool started_ = false;
};

} // namespace fiji::controller
