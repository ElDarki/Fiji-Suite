#pragma once

// VIEW layer: the Transitions dock. Shows the transitions of the canvas the Scenes dock shows (through the
// controller), its duration, and buttons to add, remove, rename and configure them. Texts and the name prompt come
// from the caller.

#include <QFrame>
#include <QString>
#include <functional>

class QComboBox;
class QPushButton;
class QSpinBox;

namespace fiji::controller {
class TransitionsController;
}

namespace fiji::view {

struct TransitionsTexts {
	QString duration, add, remove, properties, copyFromMain, confirmRemoveTitle,
		confirmRemoveText /* %1: the name */, rename, openProperties, nameTitle;
};

// Asks for a name; false when cancelled. name holds the suggestion and receives the answer.
using AskName = std::function<bool(QWidget *parent, const QString &title, QString &name)>;

class TransitionsDock : public QFrame {
	Q_OBJECT

public:
	TransitionsDock(controller::TransitionsController &controller, const TransitionsTexts &texts, AskName askName,
			QWidget *parent = nullptr);

private:
	void rebuild();
	void showCurrent(const QString &name);
	void updateButtons();
	bool askUniqueName(QString &name, const QString &renaming = QString());
	void showAddMenu();
	void showPropertiesMenu();
	void confirmRemove();

	controller::TransitionsController &controller_;
	TransitionsTexts texts_;
	AskName askName_;
	QComboBox *list_ = nullptr;
	QSpinBox *duration_ = nullptr;
	QPushButton *remove_ = nullptr;
	QPushButton *properties_ = nullptr;
};

} // namespace fiji::view
