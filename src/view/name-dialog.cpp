#include "name-dialog.hpp"

#include <QDialogButtonBox>
#include <QVBoxLayout>

namespace fiji::view {

NameDialog::NameDialog(QWidget *parent, const QString &title) : QDialog(parent)
{
	setWindowTitle(title);
	setModal(true);
	setWindowModality(Qt::WindowModality::WindowModal);
	setMinimumWidth(200);
	setMinimumHeight(100);
	QVBoxLayout *layout = new QVBoxLayout;
	setLayout(layout);

	userText = new QLineEdit(this);
	layout->addWidget(userText);

	QDialogButtonBox *buttonbox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	layout->addWidget(buttonbox);
	buttonbox->setCenterButtons(true);
	connect(buttonbox, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttonbox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

std::string NameDialog::CleanName(std::string str)
{
	auto isWhitespace = [](char ch) {
		return ch == ' ' || ch == '\t';
	};
	while (!str.empty() && isWhitespace(str.back()))
		str.erase(str.end() - 1);
	while (!str.empty() && isWhitespace(str.front()))
		str.erase(str.begin());
	return str;
}

bool fiji::view::NameDialog::AskForName(QWidget *parent, const QString &title, std::string &name)
{
	NameDialog dialog(parent, title);
	dialog.userText->setMaxLength(170);
	dialog.userText->setText(QString::fromUtf8(name.c_str()));
	dialog.userText->selectAll();

	if (dialog.exec() != DialogCode::Accepted) {
		return false;
	}
	name = CleanName(dialog.userText->text().toUtf8().constData());
	return true;
}

} // namespace fiji::view
