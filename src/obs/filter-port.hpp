#pragma once

// The OBS port for the Filters dock: what can be done to the filters of the shown source. Free of obs.h; the OBS
// implementation is obs::ObsFilters (obs-filters.hpp), which also keeps model::FilterList in step with OBS.

#include "../model/filters.hpp"

namespace fiji::obs {

class FilterPort {
public:
	virtual ~FilterPort() = default;

	virtual model::MediaFlags sourceFlags() = 0;          // of the shown source
	virtual QList<model::FilterType> filterTypes() = 0;   // every registered filter type
	virtual QString add(const QString &typeId, const QString &name) = 0;   // the new filter's key, or empty
	virtual QString duplicate(const QString &key, const QString &name) = 0; // the copy's key, or empty
	virtual void remove(const QString &key) = 0;           // undoable from OBS's Edit menu
	virtual void rename(const QString &key, const QString &name) = 0;
	virtual void setEnabled(const QString &key, bool enabled) = 0;
	virtual void move(const QString &key, model::FilterMove move) = 0;
};

} // namespace fiji::obs
