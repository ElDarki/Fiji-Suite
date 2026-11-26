#pragma once

// Outputs that can be started one at a time, as the output sequencer needs them. Free of obs.h. Ids are
// "main:stream", "main:record", "main:backtrack", "main:virtual_cam" (OBS's own outputs) and "output:<name>" (the
// plugin's). Implemented by the output dock, which owns the outputs today.

#include <QString>
#include <functional>

namespace fiji::obs {

class StartableOutputs {
public:
	virtual ~StartableOutputs() = default;
	// Starts the output. True when it accepted (onLive then runs once on the UI thread when it is live, or has
	// stopped trying), false when it cannot start now. An output that is already live accepts and calls onLive at once.
	virtual bool startOutput(const QString &id, std::function<void()> onLive) = 0;
};

} // namespace fiji::obs
