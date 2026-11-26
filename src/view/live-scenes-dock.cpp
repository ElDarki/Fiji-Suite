#include "live-scenes-dock.hpp"

#include "../controller/live-scenes-controller.hpp"
#include "../model/live-scenes.hpp"

#include <QCursor>
#include <QIcon>
#include <QListWidget>
#include <QMenu>
#include <QSignalBlocker>
#include <QToolBar>
#include <QVBoxLayout>

namespace fiji::view {

namespace {

void styleButton(QToolBar *toolbar, QAction *action, const char *themeId, const char *cls)
{
	if (auto widget = toolbar->widgetForAction(action)) {
		widget->setProperty("themeID", QString::fromUtf8(themeId));
		widget->setProperty("class", cls);
	}
}

} // namespace

LiveScenesDock::LiveScenesDock(model::LiveScenes &scenes, controller::LiveScenesController &controller,
			       const LiveScenesTexts &texts, QWidget *parent)
	: QFrame(parent),
	  scenes_(scenes),
	  controller_(controller)
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
	connect(list_, &QListWidget::currentRowChanged, this, [this](int row) {
		if (row >= 0) {
			controller_.show(row);
			if (auto item = list_->item(row); item && !item->isSelected()) {
				item->setSelected(true);
			}
		}
	});
	connect(list_, &QListWidget::itemSelectionChanged, this, [this] {
		if (auto item = list_->currentItem(); item && !item->isSelected()) {
			item->setSelected(true);
		}
	});
	layout->addWidget(list_, 1);

	auto toolbar = new QToolBar();
	toolbar->setObjectName(QStringLiteral("scenesToolbar"));
	toolbar->setIconSize(QSize(16, 16));
	toolbar->setFloatable(false);

	auto a = toolbar->addAction(QIcon(QStringLiteral(":/res/images/plus.svg")), texts.add, [this] {
		QMenu menu;
		for (const auto &name : controller_.addable()) {
			menu.addAction(name, [this, name] { controller_.add(name); });
		}
		menu.exec(QCursor::pos());
	});
	styleButton(toolbar, a, "addIconSmall", "icon-plus");

	a = toolbar->addAction(QIcon(QStringLiteral(":/res/images/minus.svg")), texts.remove,
			       [this] { controller_.removeAt(list_->currentRow()); });
	styleButton(toolbar, a, "removeIconSmall", "icon-minus");
	a->setShortcutContext(Qt::WidgetWithChildrenShortcut);
#ifdef __APPLE__
	a->setShortcut({Qt::Key_Backspace});
#else
	a->setShortcut({Qt::Key_Delete});
#endif
	list_->addAction(a);

	toolbar->addSeparator();
	a = toolbar->addAction(QIcon(QStringLiteral(":/res/images/up.svg")), texts.moveUp, [this] { move(-1); });
	styleButton(toolbar, a, "upArrowIconSmall", "icon-up");
	a = toolbar->addAction(QIcon(QStringLiteral(":/res/images/down.svg")), texts.moveDown, [this] { move(1); });
	styleButton(toolbar, a, "downArrowIconSmall", "icon-down");
	layout->addWidget(toolbar, 0);

	connect(&scenes_, &model::LiveScenes::changed, this, &LiveScenesDock::rebuild);
	connect(&scenes_, &model::LiveScenes::liveChanged, this, &LiveScenesDock::showLive);
	rebuild();
}

void LiveScenesDock::rebuild()
{
	const QSignalBlocker blocker(list_);
	const int current = list_->currentRow();
	list_->clear();
	for (const auto &entry : scenes_.entries()) {
		list_->addItem(entry.name);
	}
	const int live = scenes_.live();
	const int select = live >= 0 ? live : (current < list_->count() ? current : -1);
	list_->setCurrentRow(select);
}

void LiveScenesDock::showLive(int index)
{
	const QSignalBlocker blocker(list_);
	list_->setCurrentRow(index);
}

// The moved entry stays selected, as before (it need not be the one on program).
void LiveScenesDock::move(int delta)
{
	const int target = controller_.move(list_->currentRow(), delta);
	if (target >= 0) {
		const QSignalBlocker blocker(list_);
		list_->setCurrentRow(target);
	}
}

} // namespace fiji::view
