#include "stats-controller.hpp"

#include "../obs/stats-port.hpp"

namespace fiji::controller {

StatsController::StatsController(obs::StatsSource &source, model::OutputStats &stats) : source_(source), stats_(stats) {}

model::StatsUpdate StatsController::tick(bool visible)
{
	model::StatsUpdate update;
	if (visible != active_) {
		active_ = visible;
		if (!active_ && !stats_.rows().isEmpty()) {
			stats_.clear();
			update.reset = true;
		}
	}
	if (!active_) {
		return update;
	}
	return stats_.update(source_.sample());
}

} // namespace fiji::controller
