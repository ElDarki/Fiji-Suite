#include "filters.hpp"

#include <algorithm>

namespace fiji::model {

bool filterFits(const MediaFlags &source, const MediaFlags &filter)
{
	if (filter.audio && !source.audio) {
		return false;
	}
	if (filter.video && !source.async && filter.async) {
		return false;
	}
	if (filter.video && !source.video) {
		return false;
	}
	return true;
}

QList<FilterType> addableFilters(const QList<FilterType> &types, const MediaFlags &source)
{
	QList<FilterType> out;
	for (const auto &type : types) {
		if (!type.retired && filterFits(source, type.flags)) {
			out.append(type);
		}
	}
	std::stable_sort(out.begin(), out.end(), [](const FilterType &a, const FilterType &b) {
		return a.displayName.compare(b.displayName, Qt::CaseInsensitive) < 0;
	});
	return out;
}

QString uniqueFilterName(const QString &base, const QStringList &taken)
{
	QString name = base;
	for (int i = 2; taken.contains(name); i++) {
		name = QStringLiteral("%1 %2").arg(base).arg(i);
	}
	return name;
}

FilterList::FilterList(QObject *parent) : QObject(parent) {}

int FilterList::indexOf(const QString &key) const
{
	for (int i = 0; i < entries_.size(); i++) {
		if (entries_[i].key == key) {
			return i;
		}
	}
	return -1;
}

QStringList FilterList::names() const
{
	QStringList names;
	for (const auto &f : entries_) {
		names.append(f.name);
	}
	return names;
}

void FilterList::show(const QList<Filter> &filters)
{
	hasSource_ = true;
	entries_ = filters;
	emit reset();
}

void FilterList::clear()
{
	hasSource_ = false;
	entries_.clear();
	emit reset();
}

void FilterList::added(const Filter &filter)
{
	if (!hasSource_ || indexOf(filter.key) >= 0) {
		return;
	}
	entries_.append(filter);
	emit filterAdded(static_cast<int>(entries_.size()) - 1);
}

void FilterList::removed(const QString &key)
{
	const int index = indexOf(key);
	if (index >= 0) {
		entries_.removeAt(index);
		emit filterRemoved(index);
	}
}

void FilterList::renamed(const QString &key, const QString &name)
{
	const int index = indexOf(key);
	if (index >= 0 && entries_[index].name != name) {
		entries_[index].name = name;
		emit filterChanged(index);
	}
}

void FilterList::enabledChanged(const QString &key, bool enabled)
{
	const int index = indexOf(key);
	if (index >= 0 && entries_[index].enabled != enabled) {
		entries_[index].enabled = enabled;
		emit filterChanged(index);
	}
}

void FilterList::reordered(const QList<Filter> &filters)
{
	if (hasSource_) {
		entries_ = filters;
		emit reset();
	}
}

} // namespace fiji::model
