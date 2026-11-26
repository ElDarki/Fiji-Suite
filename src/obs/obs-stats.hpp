#pragma once

// OBS adapter: reads OBS's encoders, outputs and canvases into a model::StatsSnapshot.

#include "stats-port.hpp"

namespace fiji::obs {

class ObsStatsSource final : public StatsSource {
public:
	model::StatsSnapshot sample() override;
};

} // namespace fiji::obs
