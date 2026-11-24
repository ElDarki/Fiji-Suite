#pragma once

// VIEW: asks for a name (a scene, source, transition, output, group, ...). The caller passes the translated title.

#include <QDialog>
#include <QLineEdit>
#include <string>

namespace fiji::view {

class NameDialog : public QDialog {
	Q_OBJECT
public:
	// True when the user accepted; `name` then holds the text without leading or trailing spaces and tabs.
	static bool AskForName(QWidget *parent, const QString &title, std::string &name);
	// The text without leading or trailing spaces and tabs (what AskForName returns).
	static std::string CleanName(std::string name);

private:
	NameDialog(QWidget *parent, const QString &title);
	QLineEdit *userText;
};

} // namespace fiji::view
