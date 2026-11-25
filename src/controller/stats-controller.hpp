#pragma once

// CONTROLLER layer: the Stats dock's clock. Each tick samples OBS while the dock is visible; once it is hidden, the
// rows go (they come back from OBS's counters when it shows again).

#include "../model/output-stats.hpp"

namespace fiji::obs {
class StatsSource;
}

namespace fiji::controller {

class StatsController {
public:
	StatsController(obs::StatsSource &source, model::OutputStats &stats);

	model::StatsUpdate tick(bool visible);

private:
	obs::StatsSource &source_;
	model::OutputStats &stats_;
	bool active_ = false;
};

} // namespace fiji::controller
