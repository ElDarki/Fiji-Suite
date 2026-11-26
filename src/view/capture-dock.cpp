#include "capture-dock.hpp"

#include "../controller/capture-controller.hpp"
#include "../model/captures.hpp"

#include <QAction>
#include <QCursor>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMenu>
#include <QScrollArea>
#include <QToolButton>
#include <QVBoxLayout>

namespace fiji::view {

CaptureDock::CaptureDock(model::Captures &captures, controller::CaptureController &controller, const QString &configText,
			 QWidget *parent)
	: QFrame(parent),
	  captures_(captures),
	  controller_(controller),
	  configText_(configText)
{
	auto layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);
	auto scroll = new QScrollArea;
	scroll->setWidgetResizable(true);
	layout->addWidget(scroll);

	auto content = new QWidget;
	content->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
	scroll->setWidget(content);
	rows_ = new QVBoxLayout(content);
	rows_->setContentsMargins(0, 0, 0, 0);

	for (int i = 0; i < captures_.entries().size(); i++) {
		rows_->addWidget(makeRow(i));
	}
	connect(&captures_, &model::Captures::added, this, [this](int index) { rows_->insertWidget(index, makeRow(index)); });
	connect(&captures_, &model::Captures::removed, this, [this](int index) {
		if (auto item = rows_->takeAt(index)) {
			if (auto widget = item->widget()) {
				widget->deleteLater();
			}
			delete item;
		}
	});
	connect(&captures_, &model::Captures::changed, this, &CaptureDock::refresh);
}

QWidget *CaptureDock::makeRow(int index)
{
	const auto capture = captures_.entries().value(index);
	auto row = new QFrame;
	row->setProperty("captureKey", capture.key);
	auto layout = new QVBoxLayout(row);
	auto top = new QHBoxLayout;
	layout->addLayout(top);

	auto action = new QAction(QIcon(QStringLiteral(":/res/images/cogs.svg")), configText_, row);
	auto button = new QToolButton;
	button->setIcon(QIcon(QStringLiteral(":/res/images/cogs.svg")));
	button->setText(configText_);
	button->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
	button->setDefaultAction(action);
	button->setProperty("themeID", "cogsIcon");
	button->setProperty("class", "icon-gear");
	const QString key = capture.key;
	connect(button, &QToolButton::triggered, row, [this, key] {
		QMenu menu;
		for (const auto &choice : controller_.windows(key)) {
			menu.addAction(choice.label, [this, key, choice] { controller_.choose(key, choice); });
		}
		menu.exec(QCursor::pos());
	});
	top->addWidget(button);

	auto name = new QLabel(capture.name);
	name->setObjectName(QStringLiteral("captureName"));
	top->addWidget(name);
	auto hooked = new QLabel(capture.hooked);
	hooked->setObjectName(QStringLiteral("captureHooked"));
	layout->addWidget(hooked);
	return row;
}

void CaptureDock::refresh(int index)
{
	auto item = rows_->itemAt(index);
	auto row = item ? item->widget() : nullptr;
	if (!row) {
		return;
	}
	const auto capture = captures_.entries().value(index);
	if (auto name = row->findChild<QLabel *>(QStringLiteral("captureName"))) {
		name->setText(capture.name);
	}
	if (auto hooked = row->findChild<QLabel *>(QStringLiteral("captureHooked"))) {
		hooked->setText(capture.hooked);
	}
}

} // namespace fiji::view
