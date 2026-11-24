#include "live-scenes-dock.hpp"

#include <QCursor>
#include <QListWidget>
#include <QMenu>
#include <QSignalBlocker>
#include <QToolBar>
#include <QVBoxLayout>

namespace fiji::view {

namespace {
void markButton(QToolBar *toolbar, QAction *action, const char *themeId, const char *cssClass)
{
	if (auto button = toolbar->widgetForAction(action)) {
		button->setProperty("themeID", QString::fromUtf8(themeId));
		button->setProperty("class", QString::fromUtf8(cssClass));
	}
}
} // namespace

LiveScenesDock::LiveScenesDock(controller::LiveScenesController &scenes, const Texts &texts, QWidget *parent)
	: QFrame(parent),
	  scenes_(scenes)
{
	setMinimumWidth(100);
	setMinimumHeight(50);
	setObjectName(QStringLiteral("contextContainer"));
	setContentsMargins(0, 0, 0, 0);

	auto layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);

	list_ = new QListWidget;
	list_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
	list_->setFrameShape(QFrame::NoFrame);
	list_->setFrameShadow(QFrame::Plain);
	list_->setSelectionMode(QAbstractItemView::SingleSelection);
	// the user picked an entry: it goes on program (the model then selects it)
	connect(list_, &QListWidget::currentRowChanged, this, [this](int row) {
		if (row >= 0) {
			scenes_.pick(row);
		}
	});
	layout->addWidget(list_, 1);

	auto toolbar = new QToolBar;
	toolbar->setObjectName(QStringLiteral("scenesToolbar"));
	toolbar->setIconSize(QSize(16, 16));
	toolbar->setFloatable(false);
	auto add = toolbar->addAction(QIcon(":/res/images/plus.svg"), texts.add, this, [this] {
		QMenu menu;
		fillAddMenu(menu);
		menu.exec(QCursor::pos());
	});
	markButton(toolbar, add, "addIconSmall", "icon-plus");
	auto remove = toolbar->addAction(QIcon(":/res/images/minus.svg"), texts.remove, this, [this] { removeCurrent(); });
	markButton(toolbar, remove, "removeIconSmall", "icon-minus");
	remove->setShortcutContext(Qt::WidgetWithChildrenShortcut);
#ifdef __APPLE__
	remove->setShortcut({Qt::Key_Backspace});
#else
	remove->setShortcut({Qt::Key_Delete});
#endif
	list_->addAction(remove);
	toolbar->addSeparator();
	auto up = toolbar->addAction(QIcon(":/res/images/up.svg"), texts.moveUp, this, [this] { moveCurrent(-1); });
	markButton(toolbar, up, "upArrowIconSmall", "icon-up");
	auto down = toolbar->addAction(QIcon(":/res/images/down.svg"), texts.moveDown, this, [this] { moveCurrent(1); });
	markButton(toolbar, down, "downArrowIconSmall", "icon-down");
	layout->addWidget(toolbar, 0);

	connect(&scenes_.model(), &model::LiveScenesModel::changed, this, &LiveScenesDock::refresh);
	refresh();
}

void LiveScenesDock::refresh()
{
	const QSignalBlocker block(list_); // showing the model is not a pick
	const auto scenes = scenes_.model().scenes();
	while (list_->count() > scenes.size()) {
		delete list_->takeItem(list_->count() - 1);
	}
	for (int i = 0; i < scenes.size(); i++) {
		if (i < list_->count()) {
			list_->item(i)->setText(scenes.at(i).name);
		} else {
			list_->addItem(scenes.at(i).name);
		}
	}
	const int current = scenes_.model().current();
	list_->setCurrentRow(current);
	if (current < 0) {
		list_->clearSelection();
	}
}

void LiveScenesDock::fillAddMenu(QMenu &menu)
{
	for (const auto &name : scenes_.sceneNames()) {
		menu.addAction(name, this, [this, name] { scenes_.addScene(name); });
	}
}

void LiveScenesDock::removeCurrent()
{
	scenes_.removeAt(list_->currentRow());
}

void LiveScenesDock::moveCurrent(int delta)
{
	const int row = list_->currentRow();
	if (scenes_.move(row, delta) && scenes_.model().current() != row + delta) {
		// the moved entry is not on program: keep it selected where it went
		const QSignalBlocker block(list_);
		list_->setCurrentRow(row + delta);
	}
}

} // namespace fiji::view
