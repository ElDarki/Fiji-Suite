#pragma once

// The OBS port. Controllers drive libobs only through this interface, so they can be tested with FakeObs.
// This header must stay free of obs.h; only the implementation (src/obs/real-obs.*) includes libobs.
// Scaffold: the first calls the bridge commands need; it grows with each controller moved over.

#include <QString>
#include <QStringList>

namespace fiji::obs {

class ObsApi {
public:
	virtual ~ObsApi() = default;

	virtual QStringList canvases() const = 0;
	virtual QStringList scenes(const QString &canvas) const = 0;
	virtual QString programScene(const QString &canvas) const = 0;
	virtual bool switchScene(const QString &scene, const QString &canvas) = 0;

	virtual QStringList transitions(const QString &canvas) const = 0;
	virtual bool setTransition(const QString &canvas, const QString &transition) = 0;

	virtual bool startOutput(const QString &name) = 0;
	virtual bool stopOutput(const QString &name) = 0;

	// Why the last call returned false, in words for people.
	virtual QString lastError() const = 0;
};

} // namespace fiji::obs
