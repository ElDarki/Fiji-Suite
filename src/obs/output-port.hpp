#pragma once

// The OBS side of the outputs, as the output controller needs it. Free of obs.h.

#include "../model/obs-status.hpp"
#include "obs-api.hpp"

namespace fiji::obs {

class OutputBackend {
public:
	virtual ~OutputBackend() = default;
	virtual QList<model::OutputStatus> outputs() = 0;
	virtual ObsCall startAll() = 0;
	virtual ObsCall stopAll() = 0;
	virtual ObsCall startGroup(const QString &group) = 0;
	virtual ObsCall stopGroup(const QString &group) = 0;
	virtual ObsCall startMain(const QString &kind) = 0; // stream, record, backtrack, virtual_cam
	virtual ObsCall stopMain(const QString &kind) = 0;
	virtual ObsCall startOutput(const QString &name) = 0;
	virtual ObsCall stopOutput(const QString &name) = 0;
};

} // namespace fiji::obs
