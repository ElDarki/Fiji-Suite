#pragma once

// The OBS port for the Stats dock: OBS's raw counters, once a second. Free of obs.h; the OBS implementation is
// obs::ObsStatsSource (obs-stats.hpp).

#include "../model/output-stats.hpp"

namespace fiji::obs {

class StatsSource {
public:
	virtual ~StatsSource() = default;
	virtual model::StatsSnapshot sample() = 0;
};

} // namespace fiji::obs
