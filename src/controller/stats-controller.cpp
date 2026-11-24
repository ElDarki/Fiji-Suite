#include "stats-controller.hpp"

namespace fiji::controller {

StatsController::StatsController(model::OutputStatsTable &table, obs::StatsSource &source, QObject *parent)
	: QObject(parent),
	  table_(table),
	  source_(source)
{
	connect(&timer_, &QTimer::timeout, this, &StatsController::tick);
}

void StatsController::start(int intervalMs)
{
	timer_.start(intervalMs);
}

void StatsController::tick()
{
	if (shown_ && !shown_()) {
		table_.clear(); // nobody looks: no sampling, and the rates start fresh when shown again
		return;
	}
	table_.update(source_.sample());
}

} // namespace fiji::controller
