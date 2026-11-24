#pragma once

// libobs implementation of StatsSource: obs_enum_encoders, obs_enum_outputs and obs_enum_canvases.

#include "stats-ports.hpp"

namespace fiji::obs {

class ObsStatsSource final : public StatsSource {
public:
	model::StatsSnapshot sample() override;
};

} // namespace fiji::obs
