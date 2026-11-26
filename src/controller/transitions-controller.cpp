#include "transitions-controller.hpp"

#include "../obs/transition-port.hpp"

namespace fiji::controller {

TransitionsController::TransitionsController(obs::TransitionCatalog &catalog, obs::TransitionSet &main, QObject *parent)
	: QObject(parent),
	  catalog_(catalog),
	  main_(main),
	  set_(&main)
{
}

void TransitionsController::use(obs::TransitionSet *set, QObject *owner)
{
	set_ = set;
	owner_ = owner;
	owned_ = owner != nullptr;
	emit listChanged();
	emit currentChanged(current());
	emit durationChanged(duration());
}

void TransitionsController::useMain()
{
	use(&main_);
}

obs::TransitionSet *TransitionsController::set() const
{
	if (owned_ && !owner_) {
		return nullptr; // its canvas dock is gone
	}
	return set_;
}

bool TransitionsController::hasSet() const
{
	return set() != nullptr;
}

bool TransitionsController::ownList() const
{
	auto s = set();
	return s && s->ownList();
}

QList<model::TransitionInfo> TransitionsController::transitions() const
{
	auto s = set();
	return s ? s->transitions() : QList<model::TransitionInfo>();
}

QString TransitionsController::current() const
{
	auto s = set();
	return s ? s->current() : QString();
}

bool TransitionsController::configurable(const QString &name) const
{
	for (const auto &t : transitions()) {
		if (t.name == name) {
			return t.configurable;
		}
	}
	return false;
}

void TransitionsController::select(const QString &name)
{
	auto s = set();
	if (s && !name.isEmpty() && s->current() != name) {
		s->select(name);
	}
}

QList<model::CopyChoice> TransitionsController::copyChoices() const
{
	if (!ownList()) {
		return {};
	}
	return model::copyChoices(main_.transitions(), transitions());
}

bool TransitionsController::copyFromMain(const QString &name)
{
	for (const auto &choice : copyChoices()) {
		if (choice.name == name && choice.enabled) {
			if (set()->copyFromMain(name)) {
				emit listChanged();
				emit currentChanged(current());
				return true;
			}
			return false;
		}
	}
	return false;
}

QList<model::TransitionType> TransitionsController::types() const
{
	return catalog_.types();
}

model::TransitionNameProblem TransitionsController::nameProblem(const QString &name, const QString &renaming) const
{
	return model::transitionNameProblem(name, transitions(), renaming);
}

bool TransitionsController::addNew(const QString &typeId, const QString &name)
{
	auto s = set();
	if (!s || typeId.isEmpty()) {
		return false;
	}
	if (s->ownList() && nameProblem(name) != model::TransitionNameProblem::None) {
		return false;
	}
	if (!s->addNew(typeId, name)) {
		return false;
	}
	if (s->ownList()) {
		emit listChanged();
		emit currentChanged(current());
	}
	return true;
}

bool TransitionsController::remove(const QString &name)
{
	auto s = set();
	if (!s || !s->ownList() || !configurable(name)) {
		return false;
	}
	if (!s->remove(name)) {
		return false;
	}
	emit listChanged();
	emit currentChanged(current());
	return true;
}

bool TransitionsController::rename(const QString &name, const QString &newName)
{
	auto s = set();
	if (!s || !configurable(name) || nameProblem(newName, name) != model::TransitionNameProblem::None) {
		return false;
	}
	if (name == newName) {
		return true;
	}
	if (!s->rename(name, newName)) {
		return false;
	}
	emit listChanged();
	emit currentChanged(current());
	return true;
}

void TransitionsController::openProperties(const QString &name)
{
	auto s = set();
	if (s && configurable(name)) {
		s->openProperties(name);
	}
}

int TransitionsController::duration() const
{
	return catalog_.duration();
}

void TransitionsController::setDuration(int ms)
{
	catalog_.setDuration(ms);
}

void TransitionsController::obsListChanged()
{
	if (set() == &main_) {
		emit listChanged();
		emit currentChanged(current());
	}
}

void TransitionsController::obsCurrentChanged()
{
	emit currentChanged(current());
}

void TransitionsController::obsDurationChanged()
{
	emit durationChanged(duration());
}

} // namespace fiji::controller
