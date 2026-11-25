#pragma once

// CONTROLLER layer: what the Transitions dock does, on the transitions of the canvas the Scenes dock shows: OBS's
// own (main canvas), a canvas dock's, or none (a canvas without a dock).

#include "../model/transitions.hpp"

#include <QObject>
#include <QPointer>

namespace fiji::obs {
class TransitionSet;
class TransitionCatalog;
} // namespace fiji::obs

namespace fiji::controller {

class TransitionsController : public QObject {
	Q_OBJECT

public:
	TransitionsController(obs::TransitionCatalog &catalog, obs::TransitionSet &main, QObject *parent = nullptr);

	// The set to work on. owner: the object that holds it (a canvas dock); when it goes away, so does the set.
	void use(obs::TransitionSet *set, QObject *owner = nullptr);
	void useMain();

	bool hasSet() const;
	bool ownList() const;
	QList<model::TransitionInfo> transitions() const;
	QString current() const;
	bool configurable(const QString &name) const;

	void select(const QString &name);
	QList<model::CopyChoice> copyChoices() const; // empty unless a canvas dock's list
	bool copyFromMain(const QString &name);
	QList<model::TransitionType> types() const;
	model::TransitionNameProblem nameProblem(const QString &name, const QString &renaming = QString()) const;
	bool addNew(const QString &typeId, const QString &name);
	bool remove(const QString &name);
	bool rename(const QString &name, const QString &newName);
	void openProperties(const QString &name);
	int duration() const;
	void setDuration(int ms);

	// OBS's news.
	void obsListChanged();     // OBS's own list changed
	void obsCurrentChanged();  // the current transition changed
	void obsDurationChanged();

signals:
	void listChanged();
	void currentChanged(const QString &name);
	void durationChanged(int ms);

private:
	obs::TransitionSet *set() const;

	obs::TransitionCatalog &catalog_;
	obs::TransitionSet &main_;
	obs::TransitionSet *set_ = nullptr;
	QPointer<QObject> owner_;
	bool owned_ = false;
};

} // namespace fiji::controller
