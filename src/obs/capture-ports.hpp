#pragma once

// OBS port: the game and window capture sources, for the capture dock (controller::CaptureController). Qt Core only.

#include <QList>
#include <QString>
#include <QVariant>
#include <functional>

namespace fiji::obs {

// One window a capture source can be pointed at: its title as OBS lists it, and the value OBS stores for it.
struct CaptureWindow {
	QString title;
	QVariant value;
};

class CaptureSources {
public:
	// Called from any thread (libobs signals).
	struct Listener {
		std::function<void(const QString &uuid, const QString &name)> added;
		std::function<void(const QString &uuid)> removed;
		std::function<void(const QString &uuid, const QString &name)> renamed;
		// What the source captures now; both empty when it lets go.
		std::function<void(const QString &uuid, const QString &executable, const QString &title)> hooked;
	};

	virtual ~CaptureSources() = default;
	// Starts reporting (once; game_capture and window_capture sources only).
	virtual void listen(Listener listener) = 0;
	virtual QList<CaptureWindow> windows(const QString &uuid) = 0;
	virtual void pickWindow(const QString &uuid, const QVariant &value) = 0;
};

} // namespace fiji::obs
