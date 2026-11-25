#pragma once

// CONTROLLER layer: what the Filters dock does with the shown source's filters. Opening a filter shows it in the
// Properties dock (the open callback, set by the composition while that dock is legacy).

#include "../model/filters.hpp"

#include <functional>

namespace fiji::obs {
class FilterPort;
}

namespace fiji::controller {

class FiltersController {
public:
	using Open = std::function<void(const QString &filterKey)>;
	FiltersController(model::FilterList &filters, obs::FilterPort &obs, Open open);

	bool hasSource() const;
	QList<model::FilterType> addable() const;
	bool add(const QString &typeId);       // named after its type, made unique; opened
	bool duplicate(const QString &key);    // "name 2", ...; opened
	bool remove(const QString &key);
	bool nameTaken(const QString &name, const QString &renaming) const;
	bool rename(const QString &key, const QString &name);
	void setEnabled(const QString &key, bool enabled);
	void move(const QString &key, model::FilterMove move);
	void open(const QString &key);

private:
	model::FilterList &filters_;
	obs::FilterPort &obs_;
	Open open_;
};

} // namespace fiji::controller
