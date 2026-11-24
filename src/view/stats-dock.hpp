#pragma once

// VIEW: the Stats dock. A sortable table of model::OutputStatsTable with grouped headers (Output / Encoder / Canvas),
// rate graphs drawn from each row's history, the row's canvas colour as lines, and a header menu to sort and to show
// or hide columns. The column layout is kept per dock mode in the profile's view state ("stats_state[_<mode>]").

#include "../controller/stats-controller.hpp"

#include <QFrame>
#include <QHeaderView>
#include <QStyledItemDelegate>

class QTableView;
class QMenu;

namespace fiji::model {
class ProfileModel;
}

namespace fiji::view {

class StatsDock : public QFrame {
	Q_OBJECT

public:
	StatsDock(controller::StatsController &stats, model::ProfileModel *profile, QWidget *parent = nullptr);

	// For tests.
	QTableView *table() const { return table_; }
	void fillHeaderMenu(QMenu &menu, int section);

public slots:
	// Called by name by the composition when the dock mode changes or is saved.
	void LoadMode(QString mode);
	void SaveSettings(bool closing = false, QString mode = QString());

private:
	QTableView *table_ = nullptr;
	model::ProfileModel *profile_ = nullptr;
};

// Paints a rate column's history as a graph behind its number, and the row's canvas colour as lines above and below.
class StatsGraphDelegate : public QStyledItemDelegate {
	Q_OBJECT

public:
	using QStyledItemDelegate::QStyledItemDelegate;
	void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};

// A header whose "Section\nName" titles show the sections once above their group of columns.
class GroupedHeaderView : public QHeaderView {
	Q_OBJECT

public:
	explicit GroupedHeaderView(Qt::Orientation orientation, QWidget *parent = nullptr);

protected:
	void paintSection(QPainter *painter, const QRect &rect, int logicalIndex) const override;

private:
	bool isFirstVisibleSection(int visual) const;
	bool isLastVisibleSection(int visual) const;
};

} // namespace fiji::view
