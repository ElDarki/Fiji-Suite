#include "stats-dock.hpp"

#include "text.hpp"
#include "../model/profile-model.hpp"

#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QSortFilterProxyModel>
#include <QTableView>
#include <QVBoxLayout>

namespace fiji::view {

StatsDock::StatsDock(controller::StatsController &stats, model::ProfileModel *profile, QWidget *parent)
	: QFrame(parent),
	  profile_(profile)
{
	auto layout = new QVBoxLayout(this);

	table_ = new QTableView(this);
	table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
	table_->setSortingEnabled(true);

	auto proxy = new QSortFilterProxyModel(this);
	proxy->setSourceModel(&stats.table());
	proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
	table_->setModel(proxy);
	table_->setHorizontalHeader(new GroupedHeaderView(Qt::Horizontal, table_));
	table_->setTextElideMode(Qt::ElideRight);
	table_->horizontalHeader()->setTextElideMode(Qt::ElideRight);
	table_->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
	table_->setItemDelegate(new StatsGraphDelegate(table_));
	for (int column = 0; column < model::OutputStatsTable::ColumnCount; ++column) {
		table_->setColumnHidden(column, !model::OutputStatsTable::defaultVisible(column));
	}

	table_->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(table_->horizontalHeader(), &QHeaderView::customContextMenuRequested, this, [this](const QPoint &pos) {
		QMenu menu;
		const int section = table_->horizontalHeader()->logicalIndexAt(pos);
		fillHeaderMenu(menu, section);
		if (section >= 0) {
			menu.exec(QCursor::pos());
		} else {
			menu.exec(table_->horizontalHeader()->viewport()->mapToGlobal(table_->horizontalHeader()->rect().center()));
		}
	});
	layout->addWidget(table_);

	// sample only while the dock is shown
	QPointer<StatsDock> self(this);
	stats.setShown([self] { return self && self->isVisible(); });
}

void StatsDock::fillHeaderMenu(QMenu &menu, int section)
{
	auto header = table_->horizontalHeader();
	if (section >= 0) {
		auto sort = menu.addMenu(pluginText("Sort"));
		auto ascending = sort->addAction(pluginText("Ascending"), this,
						 [this, section] { table_->sortByColumn(section, Qt::AscendingOrder); });
		ascending->setCheckable(true);
		ascending->setChecked(header->sortIndicatorSection() == section && header->sortIndicatorOrder() == Qt::AscendingOrder);
		auto descending = sort->addAction(pluginText("Descending"), this,
						  [this, section] { table_->sortByColumn(section, Qt::DescendingOrder); });
		descending->setCheckable(true);
		descending->setChecked(header->sortIndicatorSection() == section &&
				       header->sortIndicatorOrder() == Qt::DescendingOrder);
		menu.addSeparator();
	}
	const int count = table_->model()->columnCount();
	for (int i = 0; i < count; ++i) {
		auto toggle = menu.addAction(table_->model()->headerData(i, Qt::Horizontal, Qt::DisplayRole).toString(), this,
					     [this, i] { table_->setColumnHidden(i, !table_->isColumnHidden(i)); });
		toggle->setCheckable(true);
		toggle->setChecked(!table_->isColumnHidden(i));
	}
}

void StatsDock::LoadMode(QString mode)
{
	if (!profile_) {
		return;
	}
	QString state = profile_->view(QStringLiteral("stats_state_") + mode).toString();
	if (state.isEmpty()) {
		state = profile_->view(QStringLiteral("stats_state_") + mode.toLower()).toString();
	}
	if (!state.isEmpty()) {
		table_->horizontalHeader()->restoreState(QByteArray::fromBase64(state.toUtf8()));
	}
}

void StatsDock::SaveSettings(bool closing, QString mode)
{
	if (closing || !profile_) {
		return;
	}
	const QString key = mode.isEmpty() ? QStringLiteral("stats_state") : QStringLiteral("stats_state_") + mode;
	profile_->setView(key, QString::fromUtf8(table_->horizontalHeader()->saveState().toBase64()));
}

void StatsGraphDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
	const QRect rect = option.rect;
	const auto history = index.data(model::OutputStatsTable::GraphRole).value<QList<quint32>>();
	if (!history.isEmpty() && rect.width() > 1 && rect.height() > 1) {
		// the newest value at the right edge, one pixel per second, scaled to 1.2 x the largest one shown
		const int shown = std::min<int>(int(history.size()), rect.width());
		quint32 top = 0;
		for (int i = int(history.size()) - shown; i < history.size(); ++i) {
			top = std::max(top, history.at(i));
		}
		if (top > 0) {
			const double scale = rect.height() / (top * 1.2);
			QPainterPath area;
			QPolygonF line;
			const int left = rect.right() - shown + 1;
			area.moveTo(left, rect.bottom() + 1);
			for (int i = 0; i < shown; ++i) {
				const double y = rect.bottom() + 1 - history.at(int(history.size()) - shown + i) * scale;
				area.lineTo(left + i, y);
				line << QPointF(left + i, y);
			}
			area.lineTo(rect.right() + 1, rect.bottom() + 1);
			area.closeSubpath();
			painter->save();
			painter->setClipRect(rect);
			painter->fillPath(area, option.palette.highlight());
			painter->setPen(QPen(option.palette.text().color(), 1));
			painter->drawPolyline(line);
			painter->restore();
		}
	}

	QStyledItemDelegate::paint(painter, option, index);

	const QVariant color = index.data(model::OutputStatsTable::ColorRole);
	if (color.isValid()) {
		// Fiji Suite stores colours as 0xAABBGGRR
		const qint64 value = color.toLongLong();
		const QColor canvas(int(value & 0xff), int((value >> 8) & 0xff), int((value >> 16) & 0xff));
		painter->save();
		painter->setPen(QPen(canvas, 2));
		painter->drawLine(rect.left() + 1, rect.top() + 1, rect.right(), rect.top() + 1);
		painter->drawLine(rect.left() + 1, rect.bottom(), rect.right(), rect.bottom());
		painter->restore();
	}
}

GroupedHeaderView::GroupedHeaderView(Qt::Orientation orientation, QWidget *parent) : QHeaderView(orientation, parent) {}

void GroupedHeaderView::paintSection(QPainter *painter, const QRect &rect, int logicalIndex) const
{
	if (!rect.isValid()) {
		return;
	}

	// get the state of the section
	QStyleOptionHeader opt;
	initStyleOption(&opt);

	QStyle::State state = QStyle::State_None;

	if (isEnabled()) {
		state |= QStyle::State_Enabled;
	}

	if (window()->isActiveWindow()) {
		state |= QStyle::State_Active;
	}

	//if (sectionsClickable()) {
	//	if (logicalIndex == d->hover) {
	//		state |= QStyle::State_MouseOver;
	//	}
	//
	//	if (logicalIndex == pressed()) {
	//		state |= QStyle::State_Sunken;
	//
	//	} else if (d->highlightSelected) {
	//		if (d->sectionIntersectsSelection(logicalIndex)) {
	//			state |= QStyle::State_On;
	//		}
	//
	//		if (d->isSectionSelected(logicalIndex)) {
	//			state |= QStyle::State_Sunken;
	//		}
	//	}
	//}

	if (isSortIndicatorShown() && sortIndicatorSection() == logicalIndex)
		opt.sortIndicator = (sortIndicatorOrder() == Qt::AscendingOrder) ? QStyleOptionHeader::SortDown
										 : QStyleOptionHeader::SortUp;

	// setup the style options structure
	QVariant textAlignment = model()->headerData(logicalIndex, orientation(), Qt::TextAlignmentRole);
	opt.rect = QRect(rect.left(), rect.top() + opt.rect.height() / 2, rect.width(), rect.height() / 2);
	opt.section = logicalIndex;
	opt.state |= state;

	opt.textAlignment = Qt::Alignment(textAlignment.isValid() ? Qt::Alignment(textAlignment.toInt()) : defaultAlignment());

	opt.iconAlignment = Qt::AlignVCenter;
	auto text = model()->headerData(logicalIndex, orientation(), Qt::DisplayRole).toString();
	auto t = text.split("\n");
	if (t.size() < 2) {
		t.append(QString());
	}
	opt.text = t[1];

	int margin = 2 * style()->pixelMetric(QStyle::PM_HeaderMargin, nullptr, this);

	const Qt::Alignment headerArrowAlignment =
		static_cast<Qt::Alignment>(style()->styleHint(QStyle::SH_Header_ArrowAlignment, nullptr, this));
	const bool isHeaderArrowOnTheSide = headerArrowAlignment & Qt::AlignVCenter;
	if (isSortIndicatorShown() && sortIndicatorSection() == logicalIndex && isHeaderArrowOnTheSide) {
		margin += style()->pixelMetric(QStyle::PM_HeaderMarkSize, nullptr, this);
	}

	if (textElideMode() != Qt::ElideNone) {
		opt.text = opt.fontMetrics.elidedText(opt.text, textElideMode(), rect.width() - margin);
	}

	QVariant variant = model()->headerData(logicalIndex, orientation(), Qt::DecorationRole);
	opt.icon = variant.value<QIcon>();

	if (opt.icon.isNull()) {
		opt.icon = variant.value<QPixmap>();
	}

	QVariant foregroundBrush = model()->headerData(logicalIndex, orientation(), Qt::ForegroundRole);

	if (foregroundBrush.canConvert<QBrush>()) {
		opt.palette.setBrush(QPalette::ButtonText, foregroundBrush.value<QBrush>());
	}

	QPointF oldBO = painter->brushOrigin();
	QVariant backgroundBrush = model()->headerData(logicalIndex, orientation(), Qt::BackgroundRole);

	if (backgroundBrush.canConvert<QBrush>()) {
		opt.palette.setBrush(QPalette::Button, foregroundBrush.value<QBrush>());
		opt.palette.setBrush(QPalette::Window, backgroundBrush.value<QBrush>());
		painter->setBrushOrigin(opt.rect.topLeft());
	}

	// the section position
	int visual = visualIndex(logicalIndex);
	Q_ASSERT(visual != -1);

	bool first = isFirstVisibleSection(visual);
	bool last = isLastVisibleSection(visual);

	if (first && last) {
		opt.position = QStyleOptionHeader::OnlyOneSection;
	} else if (first) {
		opt.position = QStyleOptionHeader::Beginning;
	} else if (last) {
		opt.position = QStyleOptionHeader::End;
	} else {
		opt.position = QStyleOptionHeader::Middle;
	}

	opt.orientation = orientation();

	// the selected position
	//bool previousSelected = false; //d->isSectionSelected(this->logicalIndex(visual - 1));
	//bool nextSelected = false;     //d->isSectionSelected(this->logicalIndex(visual + 1));

	//if (previousSelected && nextSelected) {
	//	opt.selectedPosition = QStyleOptionHeader::NextAndPreviousAreSelected;
	//} else if (previousSelected) {
	//	opt.selectedPosition = QStyleOptionHeader::PreviousIsSelected;
	//} else if (nextSelected) {
	//	opt.selectedPosition = QStyleOptionHeader::NextIsSelected;
	//} else {
	//	opt.selectedPosition = QStyleOptionHeader::NotAdjacent;
	//}

	// draw the section
	style()->drawControl(QStyle::CE_Header, &opt, painter, this);

	auto prev_group_visual = visual;
	auto left = rect.left();
	auto li = this->logicalIndex(prev_group_visual);
	while (li >= 0 && model()->headerData(li, orientation(), Qt::DisplayRole).toString().startsWith(t[0])) {
		if (li != logicalIndex)
			left -= sectionSize(li);
		prev_group_visual--;
		li = this->logicalIndex(prev_group_visual);
		while (li >= 0 && isSectionHidden(li)) {
			prev_group_visual--;
			li = this->logicalIndex(prev_group_visual);
		}
	}
	first = li < 0;

	auto next_group_visual = prev_group_visual + 1;
	li = this->logicalIndex(next_group_visual);
	while (li >= 0 && isSectionHidden(li)) {
		next_group_visual++;
		li = this->logicalIndex(next_group_visual);
	}
	auto width = rect.width();
	while (li >= 0 && model()->headerData(li, orientation(), Qt::DisplayRole).toString().startsWith(t[0])) {
		if (li != logicalIndex)
			width += sectionSize(li);
		next_group_visual++;
		li = this->logicalIndex(next_group_visual);
		while (li >= 0 && isSectionHidden(li)) {
			next_group_visual++;
			li = this->logicalIndex(next_group_visual);
		}
	}
	last = li < 0;
	if (!opt.icon.isNull())
		opt.icon = QIcon();
	opt.textAlignment = Qt::AlignCenter;
	opt.rect = QRect(left, rect.top(), width, rect.height() / 2);
	opt.text = t[0];
	if (textElideMode() != Qt::ElideNone) {
		margin = 2 * style()->pixelMetric(QStyle::PM_HeaderMargin, nullptr, this);
		opt.text = opt.fontMetrics.elidedText(opt.text, textElideMode(), width - margin);
	}
	opt.sortIndicator = QStyleOptionHeader::SortIndicator::None;
	if (first && last) {
		opt.position = QStyleOptionHeader::OnlyOneSection;
	} else if (first) {
		opt.position = QStyleOptionHeader::Beginning;
	} else if (last) {
		opt.position = QStyleOptionHeader::End;
	} else {
		opt.position = QStyleOptionHeader::Middle;
	}
	style()->drawControl(QStyle::CE_Header, &opt, painter, this);

	painter->setBrushOrigin(oldBO);
}

bool GroupedHeaderView::isFirstVisibleSection(int visual) const
{
	if (visual < 0)
		return false;
	auto li = logicalIndex(visual);
	if (li < 0)
		return false;
	auto vi = visual - 1;
	li = logicalIndex(vi);
	while (li >= 0 && isSectionHidden(li)) {
		vi--;
		li = logicalIndex(vi);
	}
	return vi < 0;
}

bool GroupedHeaderView::isLastVisibleSection(int visual) const
{
	if (visual < 0)
		return false;
	auto li = logicalIndex(visual);
	if (li < 0)
		return false;
	auto vi = visual + 1;
	li = logicalIndex(vi);
	while (li >= 0 && isSectionHidden(li)) {
		vi++;
		li = logicalIndex(vi);
	}
	return li < 0;
}

} // namespace fiji::view
