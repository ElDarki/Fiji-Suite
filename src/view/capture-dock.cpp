#include "capture-dock.hpp"

#include <QCursor>
#include <QLabel>
#include <QMenu>
#include <QScrollArea>
#include <QToolButton>
#include <QVBoxLayout>

namespace fiji::view {

CaptureDock::CaptureDock(controller::CaptureController &captures, const QString &configText, QWidget *parent)
	: QFrame(parent),
	  captures_(captures),
	  configText_(configText)
{
	auto outer = new QVBoxLayout(this);
	outer->setContentsMargins(0, 0, 0, 0);
	outer->setSpacing(0);
	auto scroll = new QScrollArea;
	scroll->setWidgetResizable(true);
	outer->addWidget(scroll);

	auto content = new QWidget;
	content->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
	scroll->setWidget(content);
	list_ = new QVBoxLayout(content);
	list_->setContentsMargins(0, 0, 0, 0);

	for (const auto &entry : captures_.entries()) {
		add(entry);
	}
	connect(&captures_, &controller::CaptureController::added, this, &CaptureDock::add);
	connect(&captures_, &controller::CaptureController::removed, this, &CaptureDock::remove);
	connect(&captures_, &controller::CaptureController::changed, this, &CaptureDock::update);
}

void CaptureDock::add(const controller::CaptureEntry &entry)
{
	if (rows_.contains(entry.uuid)) {
		return;
	}
	Row row;
	row.frame = new QFrame;
	auto layout = new QVBoxLayout(row.frame);
	auto top = new QHBoxLayout;
	layout->addLayout(top);

	row.button = new QToolButton;
	row.button->setIcon(QIcon(":/res/images/cogs.svg"));
	row.button->setText(configText_);
	row.button->setToolTip(configText_);
	row.button->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
	row.button->setProperty("themeID", "cogsIcon");
	row.button->setProperty("class", "icon-gear");
	const QString uuid = entry.uuid;
	connect(row.button, &QToolButton::clicked, this, [this, uuid] {
		QMenu menu;
		fillWindowMenu(uuid, menu);
		menu.exec(QCursor::pos());
	});
	top->addWidget(row.button);

	row.name = new QLabel(entry.name);
	top->addWidget(row.name);
	row.capture = new QLabel(entry.capture);
	layout->addWidget(row.capture);

	list_->addWidget(row.frame);
	rows_.insert(entry.uuid, row);
}

void CaptureDock::remove(const QString &uuid)
{
	auto row = rows_.take(uuid);
	if (row.frame) {
		list_->removeWidget(row.frame);
		row.frame->deleteLater();
	}
}

void CaptureDock::update(const controller::CaptureEntry &entry)
{
	auto found = rows_.find(entry.uuid);
	if (found == rows_.end()) {
		return;
	}
	found->name->setText(entry.name);
	found->capture->setText(entry.capture);
}

void CaptureDock::fillWindowMenu(const QString &uuid, QMenu &menu)
{
	for (const auto &window : captures_.windows(uuid)) {
		const QVariant value = window.value;
		menu.addAction(window.title, this, [this, uuid, value] { captures_.pickWindow(uuid, value); });
	}
}

} // namespace fiji::view
