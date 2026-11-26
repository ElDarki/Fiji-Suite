#include "live-scenes.hpp"

#include <QJsonObject>
#include <QMutexLocker>

namespace fiji::model {

LiveScenes::LiveScenes(QObject *parent) : QObject(parent) {}

QList<LiveScene> LiveScenes::entries() const
{
	QMutexLocker lock(&mutex_);
	return entries_;
}

int LiveScenes::count() const
{
	QMutexLocker lock(&mutex_);
	return static_cast<int>(entries_.size());
}

int LiveScenes::live() const
{
	QMutexLocker lock(&mutex_);
	return live_;
}

void LiveScenes::replaceAll(const QList<LiveScene> &entries)
{
	{
		QMutexLocker lock(&mutex_);
		entries_ = entries;
	}
	setLive(-1);
	emit changed();
}

void LiveScenes::add(const LiveScene &entry)
{
	{
		QMutexLocker lock(&mutex_);
		entries_.append(entry);
	}
	emit changed();
}

bool LiveScenes::removeAt(int index)
{
	int live;
	{
		QMutexLocker lock(&mutex_);
		if (index < 0 || index >= entries_.size()) {
			return false;
		}
		entries_.removeAt(index);
		live = live_ == index ? -1 : (live_ > index ? live_ - 1 : live_);
	}
	setLive(live);
	emit changed();
	return true;
}

bool LiveScenes::removeNamed(const QString &name)
{
	if (name.isEmpty()) {
		return false;
	}
	int index = -1;
	{
		QMutexLocker lock(&mutex_);
		for (int i = 0; i < entries_.size(); i++) {
			if (entries_[i].name == name) {
				index = i;
				break;
			}
		}
	}
	return removeAt(index);
}

int LiveScenes::move(int index, int delta)
{
	int target, live;
	{
		QMutexLocker lock(&mutex_);
		target = index + delta;
		if (delta == 0 || index < 0 || index >= entries_.size() || target < 0 || target >= entries_.size()) {
			return -1;
		}
		entries_.move(index, target);
		live = live_;
		if (live == index) {
			live = target;
		} else if (live >= 0) {
			if (index < live && live <= target) {
				live--;
			} else if (target <= live && live < index) {
				live++;
			}
		}
	}
	setLive(live);
	emit changed();
	return target;
}

void LiveScenes::renamed(const QString &previous, const QString &name)
{
	bool any = false;
	{
		QMutexLocker lock(&mutex_);
		for (auto &entry : entries_) {
			if (entry.name == previous) {
				entry.name = name;
				any = true;
			}
		}
	}
	if (any) {
		emit changed();
	}
}

int LiveScenes::markLive(const QString &name, const QString &uuid)
{
	int index = -1;
	bool renamed = false;
	{
		QMutexLocker lock(&mutex_);
		for (int i = 0; i < entries_.size() && index < 0; i++) {
			if (!name.isEmpty() && entries_[i].name == name) {
				index = i;
			}
		}
		for (int i = 0; i < entries_.size() && index < 0; i++) {
			if (!uuid.isEmpty() && entries_[i].uuid == uuid) {
				index = i;
				entries_[i].name = name;
				renamed = true;
			}
		}
	}
	if (renamed) {
		emit changed();
	}
	setLive(index);
	return index;
}

QJsonArray LiveScenes::toJson() const
{
	QJsonArray array;
	for (const auto &entry : entries()) {
		array.append(QJsonObject{{"name", entry.name}, {"uuid", entry.uuid}});
	}
	return array;
}

QList<LiveScene> LiveScenes::fromJson(const QJsonArray &array)
{
	QList<LiveScene> entries;
	for (const auto &value : array) {
		const auto object = value.toObject();
		entries.append({object.value("name").toString(), object.value("uuid").toString()});
	}
	return entries;
}

void LiveScenes::setLive(int index)
{
	{
		QMutexLocker lock(&mutex_);
		if (live_ == index) {
			return;
		}
		live_ = index;
	}
	emit liveChanged(index);
}

} // namespace fiji::model
