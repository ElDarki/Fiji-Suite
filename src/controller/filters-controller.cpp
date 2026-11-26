#include "filters-controller.hpp"

#include "../obs/filter-port.hpp"

namespace fiji::controller {

FiltersController::FiltersController(model::FilterList &filters, obs::FilterPort &obs, Open open)
	: filters_(filters),
	  obs_(obs),
	  open_(std::move(open))
{
}

bool FiltersController::hasSource() const
{
	return filters_.hasSource();
}

QList<model::FilterType> FiltersController::addable() const
{
	if (!hasSource()) {
		return {};
	}
	return model::addableFilters(obs_.filterTypes(), obs_.sourceFlags());
}

bool FiltersController::add(const QString &typeId)
{
	for (const auto &type : addable()) {
		if (type.id == typeId) {
			const QString key = obs_.add(typeId, model::uniqueFilterName(type.displayName, filters_.names()));
			if (key.isEmpty()) {
				return false;
			}
			open(key);
			return true;
		}
	}
	return false;
}

bool FiltersController::duplicate(const QString &key)
{
	const int index = filters_.indexOf(key);
	if (index < 0) {
		return false;
	}
	const QString base = filters_.entries()[index].name;
	const QString copy = obs_.duplicate(key, model::uniqueFilterName(base, filters_.names()));
	if (copy.isEmpty()) {
		return false;
	}
	open(copy);
	return true;
}

bool FiltersController::remove(const QString &key)
{
	if (filters_.indexOf(key) < 0) {
		return false;
	}
	obs_.remove(key);
	return true;
}

bool FiltersController::nameTaken(const QString &name, const QString &renaming) const
{
	return name != renaming && filters_.names().contains(name);
}

bool FiltersController::rename(const QString &key, const QString &name)
{
	const int index = filters_.indexOf(key);
	if (index < 0 || name.isEmpty()) {
		return false;
	}
	const QString current = filters_.entries()[index].name;
	if (name == current) {
		return true;
	}
	if (nameTaken(name, current)) {
		return false;
	}
	obs_.rename(key, name);
	return true;
}

void FiltersController::setEnabled(const QString &key, bool enabled)
{
	if (filters_.indexOf(key) >= 0) {
		obs_.setEnabled(key, enabled);
	}
}

void FiltersController::move(const QString &key, model::FilterMove move)
{
	if (filters_.indexOf(key) >= 0) {
		obs_.move(key, move);
	}
}

void FiltersController::open(const QString &key)
{
	if (open_ && !key.isEmpty()) {
		open_(key);
	}
}

} // namespace fiji::controller
