#pragma once

// VIEW layer: the Filters dock. The filters of the source the Properties dock shows, each with its on/off box;
// selecting one opens it in the Properties dock. A toolbar and a right-click menu add, duplicate, remove, rename and
// reorder them through the controller. Texts and the name prompt come from the caller.

#include <QFrame>
#include <QString>
#include <functional>

class QListWidget;
class QListWidgetItem;
class QMenu;

namespace fiji::model {
class FilterList;
}
namespace fiji::controller {
class FiltersController;
}

namespace fiji::view {

struct FiltersTexts {
	QString add, remove, properties, moveUp, moveDown, duplicate, rename, order, moveToTop, moveToBottom,
		noSource, nameTitle;
};

class FiltersDock : public QFrame {
	Q_OBJECT

public:
	using AskName = std::function<bool(QWidget *parent, const QString &title, QString &name)>;
	FiltersDock(model::FilterList &filters, controller::FiltersController &controller, const FiltersTexts &texts,
		    AskName askName, QWidget *parent = nullptr);

private:
	QString currentKey() const;
	QString keyAt(QListWidgetItem *item) const;
	void rebuild();
	void insertRow(int index);
	void refreshRow(int index);
	void fillAddMenu(QMenu *menu);
	void showContextMenu(QListWidgetItem *item);
	void rename(const QString &key);

	model::FilterList &filters_;
	controller::FiltersController &controller_;
	FiltersTexts texts_;
	AskName askName_;
	QListWidget *list_ = nullptr;
};

} // namespace fiji::view
