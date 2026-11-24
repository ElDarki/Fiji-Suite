#pragma once

// CONTROLLER: samples OBS's counters once a second into model::OutputStatsTable while the Stats dock is shown, and
// empties the table while it is not. Qt Core only.

#include "../model/output-stats.hpp"
#include "../obs/stats-ports.hpp"

#include <QObject>
#include <QTimer>
#include <functional>

namespace fiji::controller {

class StatsController : public QObject {
	Q_OBJECT

public:
	StatsController(model::OutputStatsTable &table, obs::StatsSource &source, QObject *parent = nullptr);

	model::OutputStatsTable &table() { return table_; }
	// Whether anyone looks at the table (the dock is visible); asked on every tick.
	void setShown(std::function<bool()> shown) { shown_ = std::move(shown); }
	void start(int intervalMs = 1000);
	void stop() { timer_.stop(); }
	// One tick (the timer calls it).
	void tick();

private:
	model::OutputStatsTable &table_;
	obs::StatsSource &source_;
	std::function<bool()> shown_;
	QTimer timer_;
};

} // namespace fiji::controller
