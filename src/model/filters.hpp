#pragma once

// MODEL layer: the Filters dock. The filters of the source the Properties dock shows, and the rules for adding,
// naming and offering filter types. Qt Core only; changed on the UI thread.

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

namespace fiji::model {

// What a source makes, or what a filter works on.
struct MediaFlags {
	bool video = false;
	bool audio = false;
	bool async = false;
};

// Whether a filter of that kind can go on that source (OBS's own rule).
bool filterFits(const MediaFlags &source, const MediaFlags &filter);

struct FilterType {
	QString id;
	QString displayName;
	MediaFlags flags;
	bool retired = false; // deprecated, disabled or obsolete
};

// The add menu: the types that fit the source, by name.
QList<FilterType> addableFilters(const QList<FilterType> &types, const MediaFlags &source);

// base, or "base 2", "base 3", ... whichever is free.
QString uniqueFilterName(const QString &base, const QStringList &taken);

enum class FilterMove { Up, Down, Top, Bottom };

struct Filter {
	QString key; // the filter's uuid
	QString name;
	bool enabled = true;
};

class FilterList : public QObject {
	Q_OBJECT

public:
	explicit FilterList(QObject *parent = nullptr);

	bool hasSource() const { return hasSource_; }
	const QList<Filter> &entries() const { return entries_; }
	int indexOf(const QString &key) const;
	QStringList names() const;

	// A source is shown (with its filters in order), or none.
	void show(const QList<Filter> &filters);
	void clear();
	void added(const Filter &filter);
	void removed(const QString &key);
	void renamed(const QString &key, const QString &name);
	void enabledChanged(const QString &key, bool enabled);
	void reordered(const QList<Filter> &filters);

signals:
	void reset();          // a new list (another source, none, or a new order)
	void filterAdded(int index);
	void filterRemoved(int index);
	void filterChanged(int index);

private:
	bool hasSource_ = false;
	QList<Filter> entries_;
};

} // namespace fiji::model
