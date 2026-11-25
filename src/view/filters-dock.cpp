#include "filters-dock.hpp"

#include "../controller/filters-controller.hpp"
#include "../model/filters.hpp"

#include <QAction>
#include <QCheckBox>
#include <QCursor>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QSignalBlocker>
#include <QToolBar>
#include <QVBoxLayout>

namespace fiji::view {

namespace {

constexpr int KeyRole = Qt::UserRole;

void styleButton(QToolBar *toolbar, QAction *action, const char *themeId, const char *cls)
{
	if (auto widget = toolbar->widgetForAction(action)) {
		widget->setProperty("themeID", QString::fromUtf8(themeId));
		widget->setProperty("class", cls);
	}
}

} // namespace

FiltersDock::FiltersDock(model::FilterList &filters, controller::FiltersController &controller, const FiltersTexts &texts,
			 AskName askName, QWidget *parent)
	: QFrame(parent),
	  filters_(filters),
	  controller_(controller),
	  texts_(texts),
	  askName_(std::move(askName))
{
	setMinimumWidth(100);
	setMinimumHeight(50);
	setObjectName(QStringLiteral("contextContainer"));
	setContentsMargins(0, 0, 0, 0);

	auto layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);

	list_ = new QListWidget();
	list_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
	list_->setFrameShape(QFrame::NoFrame);
	list_->setFrameShadow(QFrame::Plain);
	list_->setSelectionMode(QAbstractItemView::SingleSelection);
	list_->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(list_, &QListWidget::customContextMenuRequested, this,
		[this](const QPoint &pos) { showContextMenu(list_->itemAt(pos)); });
	connect(list_, &QListWidget::currentItemChanged, this, [this](QListWidgetItem *item) {
		if (!item) {
			return;
		}
		controller_.open(keyAt(item));
		if (!item->isSelected()) {
			item->setSelected(true);
		}
	});
	connect(list_, &QListWidget::itemSelectionChanged, this, [this] {
		if (auto item = list_->currentItem(); item && !item->isSelected()) {
			item->setSelected(true);
		}
	});

	auto renameAction = new QAction(list_);
#ifdef __APPLE__
	renameAction->setShortcut({Qt::Key_Return});
#else
	renameAction->setShortcut({Qt::Key_F2});
#endif
	renameAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
	connect(renameAction, &QAction::triggered, this, [this] { rename(currentKey()); });
	list_->addAction(renameAction);
	layout->addWidget(list_, 1);

	auto toolbar = new QToolBar();
	toolbar->setObjectName(QStringLiteral("filtersToolbar"));
	toolbar->setIconSize(QSize(16, 16));
	toolbar->setFloatable(false);

	auto a = toolbar->addAction(QIcon(QStringLiteral(":/res/images/plus.svg")), texts_.add, [this] {
		QMenu menu;
		fillAddMenu(&menu);
		menu.exec(QCursor::pos());
	});
	styleButton(toolbar, a, "addIconSmall", "icon-plus");

	a = toolbar->addAction(QIcon(QStringLiteral(":/res/images/minus.svg")), texts_.remove,
			       [this] { controller_.remove(currentKey()); });
	styleButton(toolbar, a, "removeIconSmall", "icon-minus");
	a->setShortcutContext(Qt::WidgetWithChildrenShortcut);
#ifdef __APPLE__
	a->setShortcut({Qt::Key_Backspace});
#else
	a->setShortcut({Qt::Key_Delete});
#endif
	list_->addAction(a);

	toolbar->addSeparator();
	a = toolbar->addAction(QIcon(QStringLiteral(":/settings/images/settings/general.svg")), texts_.properties,
			       [this] { controller_.open(currentKey()); });
	styleButton(toolbar, a, "propertiesIconSmall", "icon-gear");

	toolbar->addSeparator();
	a = toolbar->addAction(QIcon(QStringLiteral(":/res/images/up.svg")), texts_.moveUp,
			       [this] { controller_.move(currentKey(), model::FilterMove::Up); });
	styleButton(toolbar, a, "upArrowIconSmall", "icon-up");
	a = toolbar->addAction(QIcon(QStringLiteral(":/res/images/down.svg")), texts_.moveDown,
			       [this] { controller_.move(currentKey(), model::FilterMove::Down); });
	styleButton(toolbar, a, "downArrowIconSmall", "icon-down");
	layout->addWidget(toolbar, 0);

	connect(&filters_, &model::FilterList::reset, this, &FiltersDock::rebuild);
	connect(&filters_, &model::FilterList::filterAdded, this, &FiltersDock::insertRow);
	connect(&filters_, &model::FilterList::filterRemoved, this, [this](int index) { delete list_->takeItem(index); });
	connect(&filters_, &model::FilterList::filterChanged, this, &FiltersDock::refreshRow);
	rebuild();
}

QString FiltersDock::keyAt(QListWidgetItem *item) const
{
	return item ? item->data(KeyRole).toString() : QString();
}

QString FiltersDock::currentKey() const
{
	return keyAt(list_->currentItem());
}

// The list again (another source, or a new order); the selected filter stays selected.
void FiltersDock::rebuild()
{
	const QString selected = currentKey();
	{
		const QSignalBlocker blocker(list_);
		list_->clear();
	}
	for (int i = 0; i < filters_.entries().size(); i++) {
		insertRow(i);
	}
	for (int i = 0; i < list_->count() && !selected.isEmpty(); i++) {
		if (keyAt(list_->item(i)) == selected) {
			const QSignalBlocker blocker(list_);
			list_->setCurrentRow(i);
			list_->item(i)->setSelected(true);
		}
	}
}

// One row: the on/off box and the name.
void FiltersDock::insertRow(int index)
{
	const auto &filter = filters_.entries()[index];
	auto item = new QListWidgetItem();
	item->setData(KeyRole, filter.key);
	list_->insertItem(index, item);

	auto row = new QWidget();
	auto box = new QCheckBox();
	box->setObjectName(QStringLiteral("filterEnabled"));
	box->setProperty("class", "checkbox-icon indicator-visibility");
	box->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
	box->setChecked(filter.enabled);
	const QString key = filter.key;
	connect(box, &QCheckBox::clicked, this, [this, key](bool on) { controller_.setEnabled(key, on); });
	auto label = new QLabel(filter.name);
	label->setObjectName(QStringLiteral("filterName"));
	label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	auto rowLayout = new QHBoxLayout(row);
	rowLayout->addWidget(box);
	rowLayout->addWidget(label);
	rowLayout->setContentsMargins(0, 0, 0, 0);
	list_->setItemWidget(item, row);
}

void FiltersDock::refreshRow(int index)
{
	auto item = list_->item(index);
	auto row = item ? list_->itemWidget(item) : nullptr;
	if (!row) {
		return;
	}
	const auto &filter = filters_.entries()[index];
	if (auto box = row->findChild<QCheckBox *>(QStringLiteral("filterEnabled")); box && box->isChecked() != filter.enabled) {
		box->setChecked(filter.enabled);
	}
	if (auto label = row->findChild<QLabel *>(QStringLiteral("filterName"))) {
		label->setText(filter.name);
	}
}

void FiltersDock::fillAddMenu(QMenu *menu)
{
	if (!controller_.hasSource()) {
		menu->addAction(texts_.noSource)->setEnabled(false);
		return;
	}
	for (const auto &type : controller_.addable()) {
		menu->addAction(type.displayName, [this, id = type.id] { controller_.add(id); });
	}
}

void FiltersDock::showContextMenu(QListWidgetItem *item)
{
	QMenu menu(this);
	auto add = menu.addMenu(texts_.add);
	connect(add, &QMenu::aboutToShow, this, [this, add] {
		add->clear();
		fillAddMenu(add);
	});
	const QString key = keyAt(item);
	if (key.isEmpty()) {
		menu.exec(QCursor::pos());
		return;
	}
	menu.addSeparator();
	menu.addAction(texts_.duplicate, [this, key] { controller_.duplicate(key); });
	menu.addAction(texts_.remove, [this, key] { controller_.remove(key); });
	menu.addAction(texts_.rename, [this, key] { rename(key); });
	auto order = menu.addMenu(texts_.order);
	order->addAction(texts_.moveUp, [this, key] { controller_.move(key, model::FilterMove::Up); });
	order->addAction(texts_.moveDown, [this, key] { controller_.move(key, model::FilterMove::Down); });
	order->addAction(texts_.moveToTop, [this, key] { controller_.move(key, model::FilterMove::Top); });
	order->addAction(texts_.moveToBottom, [this, key] { controller_.move(key, model::FilterMove::Bottom); });
	menu.exec(QCursor::pos());
}

// Asks until the name is free (or the prompt is cancelled).
void FiltersDock::rename(const QString &key)
{
	const int index = filters_.indexOf(key);
	if (index < 0 || !askName_) {
		return;
	}
	const QString current = filters_.entries()[index].name;
	QString name = current;
	while (askName_(this, texts_.nameTitle, name)) {
		if (!name.isEmpty() && !controller_.nameTaken(name, current)) {
			controller_.rename(key, name);
			return;
		}
	}
}

} // namespace fiji::view
