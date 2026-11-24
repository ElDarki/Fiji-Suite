#include "stream-output-dialog.hpp"

#include "stream-key-input.hpp"
#include "text.hpp"

#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>

namespace fiji::view {

namespace {
QString key(const QString &prefix, const char *suffix)
{
	return pluginText((prefix + QLatin1String(suffix)).toUtf8().constData());
}

QLabel *infoLabel(const QString &text)
{
	auto label = new QLabel(text);
	label->setTextFormat(Qt::RichText);
	label->setOpenExternalLinks(true);
	label->setWordWrap(true);
	label->setAlignment(Qt::AlignTop);
	return label;
}

QLabel *formLabel(const QString &text)
{
	auto label = new QLabel(text);
	label->setStyleSheet("font-weight: bold;");
	return label;
}

QPushButton *button(const QString &text)
{
	auto b = new QPushButton(text);
	b->setStyleSheet("padding: 4px 12px;");
	b->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	return b;
}
} // namespace

StreamOutputDialog::StreamOutputDialog(QWidget *parent, const QStringList &otherNames, Servers servers)
	: QDialog(parent),
	  otherNames_(otherNames),
	  servers_(std::move(servers))
{
	setModal(true);
	setContentsMargins(0, 0, 0, 0);
	setFixedSize(650, 400);
	setWindowTitle(pluginText("NewStreamOutputWindowTitle"));

	pages_ = new QStackedWidget;
	pages_->setStyleSheet("padding: 0px; margin: 0px;");
	pages_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	pages_->addWidget(platformsPage());
	for (const auto &platform : model::streamPlatforms()) {
		pageIds_.append(platform.id);
		pages_->addWidget(platformPage(platform, false));
	}
	pages_->setCurrentIndex(0);
	connect(pages_, &QStackedWidget::currentChanged, this, &StreamOutputDialog::showPage);

	auto layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(pages_, 1);
}

StreamOutputDialog::StreamOutputDialog(QWidget *parent, const model::StreamOutput &output, const QStringList &otherNames,
				       Servers servers)
	: QDialog(parent),
	  output_(output),
	  otherNames_(otherNames),
	  servers_(std::move(servers))
{
	setModal(true);
	setContentsMargins(0, 0, 0, 0);
	setFixedSize(650, 400);
	setWindowTitle(pluginText("EditStreamOutputWindowTitle"));

	const auto platform = model::streamPlatform(model::streamPlatformFor(output_.server));
	pages_ = new QStackedWidget;
	pageIds_.append(platform.id);
	pages_->addWidget(platformPage(platform, true));
	auto layout = new QVBoxLayout(this);
	layout->addWidget(pages_);
	currentConfirm_ = pageWidgets_.first().confirm;
	validate();
}

QWidget *StreamOutputDialog::platformsPage()
{
	auto page = new QWidget;
	auto layout = new QVBoxLayout(page);
	layout->setAlignment(Qt::AlignTop);
	auto description = new QLabel(pluginText("NewOutputSelectService"));
	description->setWordWrap(true);
	description->setStyleSheet("margin-bottom: 20px;");
	layout->addWidget(description);
	layout->addSpacerItem(new QSpacerItem(20, 45, QSizePolicy::Fixed, QSizePolicy::Fixed));

	auto rows = new QVBoxLayout;
	rows->setAlignment(Qt::AlignCenter);
	QHBoxLayout *row = nullptr;
	const auto platforms = model::streamPlatforms();
	for (int i = 0; i < platforms.size(); ++i) {
		if (i % 4 == 0) {
			row = new QHBoxLayout;
			rows->addLayout(row);
		}
		const auto &platform = platforms.at(i);
		auto choose = new QToolButton;
		choose->setText(platform.label.isEmpty() ? pluginText("OtherService") : platform.label);
		choose->setIcon(QIcon(platform.icon));
		choose->setIconSize(QSize(32, 32));
		choose->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
		choose->setStyleSheet("min-width: 110px; max-width: 110px; min-height: 90px; max-height: 90px; padding-top: 16px; "
				      "font-weight: bold;");
		const QString id = platform.id;
		connect(choose, &QToolButton::clicked, this, [this, id] { choosePlatform(id); });
		row->addWidget(choose);
	}
	layout->addLayout(rows);
	return page;
}

QWidget *StreamOutputDialog::platformPage(const model::StreamPlatform &platform, bool editing)
{
	using Kind = model::StreamPlatform::Server;
	Page widgets;
	auto page = new QWidget;
	page->setStyleSheet("padding: 0px; margin: 0px;");
	auto layout = new QVBoxLayout(page);
	layout->setSpacing(12);

	auto title = new QLabel(key(platform.keys, editing && platform.editTitle ? "ServiceInfoEdit" : "ServiceInfo"));
	title->setWordWrap(true);
	title->setTextFormat(Qt::RichText);
	layout->addWidget(title);

	widgets.confirm = button(pluginText(editing ? "SaveOutput" : "CreateOutput"));
	connect(widgets.confirm, &QPushButton::clicked, this, &QDialog::accept);

	auto form = new QFormLayout;
	form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
	form->setLabelAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);
	form->setSpacing(12);

	widgets.name = new QLineEdit(editing ? output_.name : key(platform.keys, "Output"));
	widgets.name->setStyleSheet("padding: 4px 8px;");
	connect(widgets.name, &QLineEdit::textEdited, this, [this](const QString &text) {
		output_.name = text;
		validate();
	});
	form->addRow(formLabel(pluginText("OutputName")), widgets.name);

	if (platform.serverKind == Kind::Choice) {
		widgets.serverList = new QComboBox;
		widgets.serverList->setMinimumHeight(30);
		widgets.serverList->setStyleSheet("padding: 4px 8px;");
		for (const auto &server : servers_ ? servers_(platform.service) : QList<model::StreamServer>()) {
			widgets.serverList->addItem(server.name, server.url);
		}
		if (editing) {
			const int found = widgets.serverList->findData(output_.server);
			if (found >= 0) {
				widgets.serverList->setCurrentIndex(found);
			}
		}
		auto list = widgets.serverList;
		connect(list, &QComboBox::currentIndexChanged, this, [this, list] {
			output_.server = list->currentData().toString();
			validate();
		});
		form->addRow(formLabel(key(platform.keys, "Server")), list);
	} else {
		widgets.serverField = new QLineEdit;
		widgets.serverField->setStyleSheet("padding: 4px 8px;");
		if (platform.serverKind == Kind::Fixed) {
			widgets.serverField->setText(platform.server);
			widgets.serverField->setDisabled(true);
			if (editing) {
				output_.server = platform.server;
			}
		} else {
			widgets.serverField->setText(editing ? output_.server : platform.server);
			auto field = widgets.serverField;
			connect(field, &QLineEdit::textEdited, this, [this](const QString &text) {
				output_.server = text;
				validate();
			});
		}
		form->addRow(formLabel(key(platform.keys, "Server")), widgets.serverField);
	}
	form->addWidget(infoLabel(key(platform.keys, "ServerInfo")));

	// the key shows only while it has focus
	auto keyField = new StreamKeyInput;
	keyField->setStyleSheet("padding: 4px 8px;");
	if (editing) {
		keyField->setText(output_.key);
	}
	keyField->setEchoMode(QLineEdit::Password);
	connect(keyField, &StreamKeyInput::focusGained, keyField, [keyField] { keyField->setEchoMode(QLineEdit::Normal); });
	connect(keyField, &StreamKeyInput::focusLost, keyField, [keyField] { keyField->setEchoMode(QLineEdit::Password); });
	connect(keyField, &QLineEdit::textEdited, this, [this](const QString &text) {
		output_.key = text;
		validate();
	});
	widgets.key = keyField;
	form->addRow(formLabel(key(platform.keys, "StreamKey")), keyField);
	form->addWidget(infoLabel(key(platform.keys, "StreamKeyInfo")));

	auto content = new QVBoxLayout;
	content->addLayout(form);
	content->addSpacerItem(new QSpacerItem(1, 20, QSizePolicy::Minimum, QSizePolicy::MinimumExpanding));
	layout->addLayout(content);

	auto controls = new QHBoxLayout;
	controls->setSpacing(0);
	controls->setContentsMargins(0, 0, 0, 0);
	if (!editing) {
		// back to the platforms, forgetting this page's values
		auto back = button(QStringLiteral("< Back"));
		connect(back, &QPushButton::clicked, this, [this] {
			output_ = {};
			pages_->setCurrentIndex(0);
		});
		controls->addWidget(back, 1);
		controls->addStretch(1);
	}
	controls->addWidget(widgets.confirm, 0, Qt::AlignRight);
	layout->addLayout(controls, 1);

	pageWidgets_.append(widgets);
	return page;
}

void StreamOutputDialog::choosePlatform(const QString &id)
{
	const int i = int(pageIds_.indexOf(id));
	if (i >= 0 && pages_->count() > i + 1) {
		pages_->setCurrentIndex(i + 1);
	}
}

StreamOutputDialog::Page StreamOutputDialog::page(const QString &id) const
{
	const int i = int(pageIds_.indexOf(id));
	return i >= 0 ? pageWidgets_.at(i) : Page();
}

void StreamOutputDialog::showPage(int index)
{
	if (index < 1 || index > pageWidgets_.size()) {
		currentConfirm_ = nullptr;
		return;
	}
	const auto &widgets = pageWidgets_.at(index - 1);
	output_.name = widgets.name->text();
	output_.server = widgets.serverList ? widgets.serverList->currentData().toString() : widgets.serverField->text();
	output_.key = widgets.key->text();
	currentConfirm_ = widgets.confirm;
	validate();
}

void StreamOutputDialog::validate()
{
	if (currentConfirm_) {
		currentConfirm_->setEnabled(model::streamOutputValid(output_, otherNames_));
	}
}

} // namespace fiji::view
