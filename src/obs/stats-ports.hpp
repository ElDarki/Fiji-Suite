#pragma once

// OBS port: one sample of OBS's encoder, output and canvas counters for the Stats dock (controller::StatsController).

#include "../model/output-stats.hpp"

namespace fiji::obs {

class StatsSource {
public:
	virtual ~StatsSource() = default;
	virtual model::StatsSnapshot sample() = 0;
};

} // namespace fiji::obs
