#pragma once

// The OBS port the outputs controller needs: the outputs the streamer has (OBS's own stream, recording, replay buffer
// and virtual camera, plus Fiji Suite's outputs), started and stopped by id. Free of obs.h.

#include <QList>
#include <QString>
#include <functional>

namespace fiji::obs {

struct OutputEntry {
	QString id;      // "main:stream", "main:record", "main:backtrack", "main:virtual_cam", or a Fiji Suite output's name
	QString group;   // the streamer's group name; empty for none
	bool main = false;
	bool stream = false; // counted by "start/stop streams"
	bool record = false; // counted by "start/stop recordings" (the replay buffer too)
};

class Outputs {
public:
	virtual ~Outputs() = default;
	virtual QList<OutputEntry> list() const = 0;
	// Starts one output. False when it cannot even try. onStarted is called once it is live or has given up (from any
	// thread), so the next one can start.
	virtual bool start(const QString &id, std::function<void()> onStarted) = 0;
	virtual void stop(const QString &id) = 0;
	virtual bool active(const QString &id) const = 0;
};

} // namespace fiji::obs
