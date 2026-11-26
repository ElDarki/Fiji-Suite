#include "transitions-dock.hpp"

#include "../controller/transitions-controller.hpp"

#include <QComboBox>
#include <QCursor>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWheelEvent>

namespace fiji::view {

namespace {

// A spin box that scrolls only once it has focus, so scrolling the dock does not change the duration.
class FocusSpinBox final : public QSpinBox {
public:
	FocusSpinBox() { setFocusPolicy(Qt::StrongFocus); }

protected:
	void wheelEvent(QWheelEvent *event) override
	{
		if (hasFocus()) {
			QSpinBox::wheelEvent(event);
		} else {
			event->ignore();
		}
	}
};

QPushButton *toolButton(const QString &text, const char *icon, const char *themeId, const char *cls)
{
	auto button = new QPushButton();
	button->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
	button->setAccessibleName(text);
	button->setToolTip(text);
	button->setIcon(QIcon(QString::fromUtf8(icon)));
	button->setProperty("themeID", themeId);
	button->setProperty("class", cls);
	button->setProperty("toolButton", true);
	button->setFlat(false);
	return button;
}

} // namespace

TransitionsDock::TransitionsDock(controller::TransitionsController &controller, const TransitionsTexts &texts,
				 AskName askName, QWidget *parent)
	: QFrame(parent),
	  controller_(controller),
	  texts_(texts),
	  askName_(std::move(askName))
{
	auto layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);

	list_ = new QComboBox();
	layout->addWidget(list_);

	auto row = new QHBoxLayout();
	row->setContentsMargins(0, 0, 0, 0);
	row->setSpacing(4);
	auto durationLabel = new QLabel(texts_.duration);
	duration_ = new FocusSpinBox();
	duration_->setMinimum(50);
	duration_->setMaximum(20000);
	duration_->setSingleStep(50);
	duration_->setSuffix(QStringLiteral("ms"));
	connect(duration_, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int ms) { controller_.setDuration(ms); });
	durationLabel->setBuddy(duration_);
	row->addWidget(durationLabel);
	row->addWidget(duration_);
	layout->addLayout(row);

	row = new QHBoxLayout();
	row->addStretch();
	auto add = toolButton(texts_.add, ":/res/images/add.png", "addIconSmall", "icon-plus");
	connect(add, &QPushButton::clicked, this, &TransitionsDock::showAddMenu);
	row->addWidget(add);
	remove_ = toolButton(texts_.remove, ":/res/images/list_remove.png", "removeIconSmall", "icon-minus");
	connect(remove_, &QPushButton::clicked, this, &TransitionsDock::confirmRemove);
	row->addWidget(remove_);
	properties_ = toolButton(texts_.properties, ":/settings/images/settings/general.svg", "menuIconSmall", "icon-dots-vert");
	connect(properties_, &QPushButton::clicked, this, &TransitionsDock::showPropertiesMenu);
	row->addWidget(properties_);
	layout->addLayout(row);
	layout->addStretch();

	connect(list_, &QComboBox::currentTextChanged, this, [this](const QString &name) {
		controller_.select(name);
		updateButtons();
	});
	connect(&controller_, &controller::TransitionsController::listChanged, this, &TransitionsDock::rebuild);
	connect(&controller_, &controller::TransitionsController::currentChanged, this, &TransitionsDock::showCurrent);
	connect(&controller_, &controller::TransitionsController::durationChanged, this, [this](int ms) {
		const QSignalBlocker blocker(duration_);
		duration_->setValue(ms);
	});
	rebuild();
	const QSignalBlocker blocker(duration_);
	duration_->setValue(controller_.duration());
}

void TransitionsDock::rebuild()
{
	{
		const QSignalBlocker blocker(list_);
		list_->clear();
		for (const auto &t : controller_.transitions()) {
			list_->addItem(t.name);
		}
	}
	showCurrent(controller_.current());
}

void TransitionsDock::showCurrent(const QString &name)
{
	{
		const QSignalBlocker blocker(list_);
		list_->setCurrentText(name);
	}
	updateButtons();
}

void TransitionsDock::updateButtons()
{
	const bool configurable = controller_.configurable(list_->currentText());
	remove_->setEnabled(configurable && controller_.ownList()); // OBS's own transitions are removed in OBS's dock
	properties_->setEnabled(configurable);
}

bool TransitionsDock::askUniqueName(QString &name, const QString &renaming)
{
	while (true) {
		if (!askName_ || !askName_(this, texts_.nameTitle, name)) {
			return false;
		}
		if (controller_.nameProblem(name, renaming) == model::TransitionNameProblem::None) {
			return true;
		}
	}
}

void TransitionsDock::showAddMenu()
{
	if (!controller_.hasSet()) {
		return;
	}
	QMenu menu(this);
	if (controller_.ownList()) {
		auto copy = menu.addMenu(texts_.copyFromMain);
		for (const auto &choice : controller_.copyChoices()) {
			auto action = copy->addAction(choice.name, [this, name = choice.name] { controller_.copyFromMain(name); });
			action->setEnabled(choice.enabled);
		}
		menu.addSeparator();
	}
	for (const auto &type : controller_.types()) {
		menu.addAction(type.displayName, [this, type] {
			QString name = type.displayName;
			if (controller_.ownList() && !askUniqueName(name)) {
				return;
			}
			controller_.addNew(type.id, name);
		});
	}
	menu.exec(QCursor::pos());
}

void TransitionsDock::confirmRemove()
{
	const QString name = list_->currentText();
	QMessageBox box(QMessageBox::Question, texts_.confirmRemoveTitle, texts_.confirmRemoveText.arg(name),
			QMessageBox::StandardButtons(QMessageBox::Yes | QMessageBox::No));
	box.setDefaultButton(QMessageBox::NoButton);
	if (box.exec() == QMessageBox::Yes) {
		controller_.remove(name);
	}
}

void TransitionsDock::showPropertiesMenu()
{
	QMenu menu(this);
	menu.addAction(texts_.rename, [this] {
		const QString current = list_->currentText();
		if (!controller_.configurable(current)) {
			return;
		}
		QString name = current;
		if (askUniqueName(name, current)) {
			controller_.rename(current, name);
		}
	});
	menu.addAction(texts_.openProperties, [this] { controller_.openProperties(list_->currentText()); });
	menu.exec(QCursor::pos());
}

} // namespace fiji::view
