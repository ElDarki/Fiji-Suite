#include "stats-dock.hpp"

#include "../controller/stats-controller.hpp"
#include "../model/output-stats.hpp"

#include <QAbstractTableModel>
#include <QColor>
#include <QCursor>
#include <QHeaderView>
#include <QImage>
#include <QMenu>
#include <QPainter>
#include <QSortFilterProxyModel>
#include <QIcon>
#include <QPixmap>
#include <QStyle>
#include <QStyleOptionHeader>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QVBoxLayout>

#include <cmath>
#include <vector>

namespace fiji::view {

namespace {

constexpr int RowRole = Qt::UserRole + 1;    // the source row, for the delegate
constexpr int CanvasRole = Qt::UserRole + 2; // the canvas colour

QColor canvasColor(qint64 packed)
{
	if (packed < 0) {
		return QColor(0, 0, 0, 0);
	}
	return QColor(packed & 0xff, (packed >> 8) & 0xff, (packed >> 16) & 0xff, 255);
}

// A rate's small graph: a strip that scrolls left by one pixel a second, scaled to the highest value seen lately.
struct Graph {
	QImage image = QImage(1, 24, QImage::Format_ARGB32);
	int width = 0;
	quint32 max = 0;
	Graph() { image.fill(0); }

	void push(quint32 value, int height, uint lineColor, uint graphColor)
	{
		if (width > 0) {
			if (value > 0 && value * 12 < max * 10) {
				image = image.copy(0, 1, image.width(), image.height() - 1)
						.scaled(image.width(), height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
				max -= max / height;
			} else if (value > max) {
				if (max > 0 && height > 0) {
					auto h = image.height() * value / max;
					image = image.copy(0, image.height() - h, image.width(), h);
				}
				max = value;
			}
			if (height > 0 && height != image.height()) {
				image = image.scaled(image.width(), height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
			}
			image = image.copy(image.width() - width + 1, 0, width, image.height());
			if (max > 0) {
				auto y = height - (int)round(height * value / (max * 1.2));
				auto x = image.width() - 1;
				for (int i = 0; i < y; i++) {
					image.setPixel(x, i, 0);
				}
				if (y < height) {
					image.setPixel(x, y, lineColor);
				}
				for (int i = y + 1; i < height; i++) {
					image.setPixel(x, i, graphColor);
				}
			}
		} else if (image.width() > 1) {
			image = image.copy(image.width(), 0, 1, image.height());
			image.fill(0);
		}
	}
};

struct RowGraphs {
	Graph outputBitrate, outputFps, encodedFps, canvasFps;
	int height = 24;
};

enum class G { None, OutputBitrate, OutputFps, EncodedFps, CanvasFps };

struct Column {
	const char *section;
	const char *name;
	bool visible;
	int alignment;
	QVariant (*value)(const model::StatsRow &row);
	G graph = G::None;
};

const int Right = Qt::AlignRight | Qt::AlignVCenter;
const int Left = Qt::AlignLeft | Qt::AlignVCenter;

QVariant resolution(quint32 w, quint32 h)
{
	return w && h ? QVariant(QString::number(w) + "x" + QString::number(h)) : QVariant();
}

const std::vector<Column> columns = {
	{"Output", "Name", true, Left, [](const model::StatsRow &r) { return r.output ? QVariant(r.outputName) : QVariant(); }},
	{"Output", "FPS", true, Right, [](const model::StatsRow &r) { return QVariant(r.outputFps); }, G::OutputFps},
	{"Output", "TotalFrames", true, Right, [](const model::StatsRow &r) { return QVariant(r.outputFrames); }},
	{"Output", "Bitrate", true, Right, [](const model::StatsRow &r) { return QVariant(r.outputBitrate); }, G::OutputBitrate},
	{"Output", "TotalData", true, Right, [](const model::StatsRow &r) { return QVariant((qulonglong)r.outputBytes); }},
	{"Encoder", "Name", false, Left, [](const model::StatsRow &r) { return r.encoder ? QVariant(r.encoderName) : QVariant(); }},
	{"Encoder", "Resolution", false, Qt::AlignCenter, [](const model::StatsRow &r) { return resolution(r.encodedWidth, r.encodedHeight); }},
	{"Encoder", "FPS", true, Right, [](const model::StatsRow &r) { return QVariant(r.encodedFps); }, G::EncodedFps},
	{"Encoder", "TotalFrames", true, Right, [](const model::StatsRow &r) { return QVariant(r.encodedFrames); }},
	{"Canvas", "Name", false, Left, [](const model::StatsRow &r) { return r.canvas ? QVariant(r.canvasName) : QVariant(); }},
	{"Canvas", "Resolution", false, Qt::AlignCenter, [](const model::StatsRow &r) { return resolution(r.canvasWidth, r.canvasHeight); }},
	{"Canvas", "FPS", true, Right, [](const model::StatsRow &r) { return QVariant(r.canvasFps); }, G::CanvasFps},
	{"Canvas", "TotalFrames", true, Right, [](const model::StatsRow &r) { return QVariant(r.canvasFrames); }},
	{"Canvas", "SkippedFrames", true, Right, [](const model::StatsRow &r) { return QVariant(r.skippedFrames); }},
};

} // namespace

// ---- the table model: the rows as Qt shows them, and their graphs ---------------------------------------------
class StatsTableModel final : public QAbstractTableModel {
public:
	StatsTableModel(model::OutputStats &stats, Translate translate, QObject *parent)
		: QAbstractTableModel(parent),
		  stats_(stats),
		  translate_(std::move(translate))
	{
	}

	std::function<int(int)> columnWidth; // the table's, for the graphs

	int rowCount(const QModelIndex & = QModelIndex()) const override { return shown_; }
	int columnCount(const QModelIndex & = QModelIndex()) const override { return (int)columns.size(); }

	QVariant data(const QModelIndex &index, int role) const override
	{
		if (!index.isValid() || index.row() >= shown_) {
			return {};
		}
		const auto &column = columns[index.column()];
		const auto &row = stats_.rows()[index.row()];
		switch (role) {
		case Qt::DisplayRole:
			return column.value(row);
		case Qt::TextAlignmentRole:
			return column.alignment;
		case Qt::UserRole:
			if (Graph *g = graph(index.row(), column.graph)) {
				g->width = columnWidth ? columnWidth(index.column()) : 0;
				return g->image;
			}
			return {};
		case RowRole:
			return index.row();
		case CanvasRole:
			return canvasColor(row.canvasColor);
		default:
			return {};
		}
	}

	QVariant headerData(int section, Qt::Orientation orientation, int role) const override
	{
		if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
			const auto &column = columns[section];
			return translate_(column.section) + "\n" + translate_(column.name);
		}
		return {};
	}

	void setRowHeight(int row, int height) const
	{
		if (row >= 0 && row < (int)graphs_.size()) {
			graphs_[row].height = height;
		}
	}

	void apply(const model::StatsUpdate &update)
	{
		if (update.reset || (int)stats_.rows().size() < shown_) {
			beginResetModel();
			shown_ = (int)stats_.rows().size();
			graphs_.assign(shown_, RowGraphs());
			endResetModel();
		}
		const int rows = (int)stats_.rows().size();
		if (rows > shown_) {
			beginInsertRows(QModelIndex(), shown_, rows - 1);
			shown_ = rows;
			graphs_.resize(rows);
			endInsertRows();
		}
		if (shown_ == 0) {
			return;
		}
		const QPalette palette;
		const uint line = palette.text().color().rgba();
		const uint fill = palette.highlight().color().rgba();
		for (int i = 0; i < shown_; i++) {
			const auto &row = stats_.rows()[i];
			auto &g = graphs_[i];
			g.outputBitrate.push(row.outputBitrate, g.height, line, fill);
			g.outputFps.push(row.outputFps, g.height, line, fill);
			g.encodedFps.push(row.encodedFps, g.height, line, fill);
			g.canvasFps.push(row.canvasFps, g.height, line, fill);
		}
		if (update.firstChanged >= 0) {
			emit dataChanged(index(update.firstChanged, 0), index(update.lastChanged, (int)columns.size() - 1),
					 {Qt::DisplayRole, Qt::UserRole});
		}
	}

private:
	Graph *graph(int row, G which) const
	{
		if (row < 0 || row >= (int)graphs_.size()) {
			return nullptr;
		}
		auto &g = graphs_[row];
		switch (which) {
		case G::OutputBitrate:
			return &g.outputBitrate;
		case G::OutputFps:
			return &g.outputFps;
		case G::EncodedFps:
			return &g.encodedFps;
		case G::CanvasFps:
			return &g.canvasFps;
		default:
			return nullptr;
		}
	}

	model::OutputStats &stats_;
	Translate translate_;
	int shown_ = 0;
	mutable std::vector<RowGraphs> graphs_;
};

namespace {

// Draws a cell's graph behind its number, and the canvas colour as lines above and below the row.
class GraphDelegate final : public QStyledItemDelegate {
public:
	GraphDelegate(StatsTableModel *model, QObject *parent) : QStyledItemDelegate(parent), model_(model) {}

	void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
	{
		const auto data = index.data(Qt::UserRole);
		if (data.canConvert<QImage>()) {
			auto image = data.value<QImage>();
			if (image.height() != option.rect.height()) {
				image = image.scaled(image.width(), option.rect.height(), Qt::IgnoreAspectRatio,
						     Qt::SmoothTransformation);
			}
			if (image.width() > option.rect.width()) {
				painter->drawImage(option.rect.left(), option.rect.top(), image, image.width() - option.rect.width());
			} else {
				painter->drawImage(option.rect.right() - image.width(), option.rect.top(), image);
			}
		}
		model_->setRowHeight(index.data(RowRole).toInt(), option.rect.height());

		QStyledItemDelegate::paint(painter, option, index);

		QPen pen = painter->pen();
		pen.setColor(index.data(CanvasRole).value<QColor>());
		pen.setWidth(2);
		painter->setPen(pen);
		painter->drawLine(option.rect.left() + 1, option.rect.top() + 1, option.rect.right(), option.rect.top() + 1);
		painter->drawLine(option.rect.left() + 1, option.rect.bottom(), option.rect.right(), option.rect.bottom());
	}

private:
	StatsTableModel *model_;
};

// Column headers in two rows: the group (Output, Encoder, Canvas) spanning its visible columns, then the column.
class GroupedHeaderView final : public QHeaderView {
public:
	explicit GroupedHeaderView(Qt::Orientation orientation, QWidget *parent = nullptr) : QHeaderView(orientation, parent) {}

protected:
	void paintSection(QPainter *painter, const QRect &rect, int logicalIndex) const override;

private:
	bool isFirstVisibleSection(int visual) const;
	bool isLastVisibleSection(int visual) const;
};

} // namespace

// ---- the dock ---------------------------------------------------------------------------------------------------
StatsDock::StatsDock(model::OutputStats &stats, controller::StatsController &controller, Translate translate,
		     QWidget *parent)
	: QFrame(parent),
	  controller_(controller),
	  translate_(translate)
{
	auto layout = new QVBoxLayout(this);

	table_ = new QTableView(this);
	table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
	table_->setSortingEnabled(true);

	model_ = new StatsTableModel(stats, translate, this);
	model_->columnWidth = [this](int column) { return table_->columnWidth(column); };
	auto proxy = new QSortFilterProxyModel(this);
	proxy->setSourceModel(model_);
	proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);

	table_->setModel(proxy);
	table_->setHorizontalHeader(new GroupedHeaderView(Qt::Horizontal, table_));
	table_->setTextElideMode(Qt::ElideRight);
	table_->horizontalHeader()->setTextElideMode(Qt::ElideRight);
	table_->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
	table_->setItemDelegate(new GraphDelegate(model_, table_));

	for (int i = 0; i < (int)columns.size(); i++) {
		if (!columns[i].visible) {
			table_->setColumnHidden(i, true);
		}
	}

	table_->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(table_->horizontalHeader(), &QHeaderView::customContextMenuRequested, this, &StatsDock::showHeaderMenu);
	layout->addWidget(table_);

	clock_.setInterval(1000);
	connect(&clock_, &QTimer::timeout, this, &StatsDock::tick);
	clock_.start();
}

void StatsDock::tick()
{
	model_->apply(controller_.tick(isVisible()));
}

QByteArray StatsDock::headerState() const
{
	return table_->horizontalHeader()->saveState();
}

void StatsDock::restoreHeaderState(const QByteArray &state)
{
	if (!state.isEmpty()) {
		table_->horizontalHeader()->restoreState(state);
	}
}

void StatsDock::showHeaderMenu(const QPoint &pos)
{
	QMenu menu;
	auto header = table_->horizontalHeader();
	const auto section = header->logicalIndexAt(pos);
	if (section >= 0) {
		auto sort = menu.addMenu(translate_("Sort"));
		auto ascending = sort->addAction(translate_("Ascending"),
						 [this, section] { table_->sortByColumn(section, Qt::SortOrder::AscendingOrder); });
		ascending->setCheckable(true);
		ascending->setChecked(header->sortIndicatorSection() == section &&
				      header->sortIndicatorOrder() == Qt::SortOrder::AscendingOrder);
		auto descending = sort->addAction(translate_("Descending"),
						  [this, section] { table_->sortByColumn(section, Qt::SortOrder::DescendingOrder); });
		descending->setCheckable(true);
		descending->setChecked(header->sortIndicatorSection() == section &&
				       header->sortIndicatorOrder() == Qt::SortOrder::DescendingOrder);
		menu.addSeparator();
	}
	const auto count = table_->model()->columnCount();
	for (int i = 0; i < count; ++i) {
		auto a = menu.addAction(table_->model()->headerData(i, Qt::Horizontal, Qt::DisplayRole).toString(),
					[this, i] { table_->setColumnHidden(i, !table_->isColumnHidden(i)); });
		a->setCheckable(true);
		a->setChecked(!table_->isColumnHidden(i));
	}
	if (section >= 0) {
		menu.exec(QCursor::pos());
	} else {
		menu.exec(header->viewport()->mapToGlobal(header->rect().center()));
	}
}

namespace {

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

} // namespace

} // namespace fiji::view
