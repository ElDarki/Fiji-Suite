#pragma once

// StatsSource for the tests: returns whatever snapshot the test set.

#include "../src/obs/stats-ports.hpp"

struct FakeStatsSource final : fiji::obs::StatsSource {
	fiji::model::StatsSnapshot next;
	int samples = 0;
	fiji::model::StatsSnapshot sample() override
	{
		samples++;
		return next;
	}
};
