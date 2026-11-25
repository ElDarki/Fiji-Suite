#pragma once

// OBS adapter for the Filters dock: watches the shown source's filters (added, removed, reordered, renamed, turned
// on or off) into model::FilterList on the UI thread, and is the FilterPort that changes them.

#include "filter-port.hpp"

#include <QHash>
#include <QObject>

struct obs_source;
struct obs_weak_source;
struct calldata;

namespace fiji::obs {

class ObsFilters final : public QObject, public FilterPort {
	Q_OBJECT

public:
	explicit ObsFilters(model::FilterList &filters, QObject *parent = nullptr);
	~ObsFilters() override;

	// The Properties dock's choice (UI thread): show this source's filters, or none.
	void show(obs_source *source);
	// That source is no longer selected (or is going away).
	void deselected(obs_source *source);
	// A filter of the shown source by key, with a new reference (or null): for the Properties dock.
	obs_source *filterSource(const QString &key) const;

	model::MediaFlags sourceFlags() override;
	QList<model::FilterType> filterTypes() override;
	QString add(const QString &typeId, const QString &name) override;
	QString duplicate(const QString &key, const QString &name) override;
	void remove(const QString &key) override;
	void rename(const QString &key, const QString &name) override;
	void setEnabled(const QString &key, bool enabled) override;
	void move(const QString &key, model::FilterMove move) override;

private:
	struct Watched;
	obs_source *shown() const; // a new reference, or null
	QList<model::Filter> readFilters();
	void watchFilters();
	void unwatchFilters();

	static void filter_add(void *data, calldata *cd);
	static void filter_remove(void *data, calldata *cd);
	static void filter_reorder(void *data, calldata *cd);
	static void source_remove(void *data, calldata *cd);
	static void filter_enable(void *data, calldata *cd);
	static void filter_rename(void *data, calldata *cd);
	static void restore_filter(const char *json);
	static void remove_filter(const char *json);

	model::FilterList &filters_;
	obs_weak_source *source_ = nullptr;
	QHash<QString, Watched *> watched_; // UI thread: the shown source's filters, by uuid
};

} // namespace fiji::obs
