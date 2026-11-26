#pragma once

// VIEW layer: the Stats dock. A table with one row per video pipeline (model::OutputStats), small graphs of the
// rates, grouped column headers (Output / Encoder / Canvas) and a menu to sort and to show or hide columns. A
// one-second clock asks the controller to sample OBS. Texts come from the caller (the plugin's locale keys).

#include <QByteArray>
#include <QFrame>
#include <QTimer>
#include <functional>

class QTableView;

namespace fiji::model {
class OutputStats;
}
namespace fiji::controller {
class StatsController;
}

namespace fiji::view {

class StatsTableModel;

using Translate = std::function<QString(const char *key)>;

class StatsDock : public QFrame {
	Q_OBJECT

public:
	StatsDock(model::OutputStats &stats, controller::StatsController &controller, Translate translate,
		  QWidget *parent = nullptr);

	// The columns' order, widths, visibility and sorting, as Qt saves them (kept per dock mode by the caller).
	QByteArray headerState() const;
	void restoreHeaderState(const QByteArray &state);

private:
	void tick();
	void showHeaderMenu(const QPoint &pos);

	controller::StatsController &controller_;
	Translate translate_;
	QTableView *table_ = nullptr;
	StatsTableModel *model_ = nullptr;
	QTimer clock_;
};

} // namespace fiji::view
