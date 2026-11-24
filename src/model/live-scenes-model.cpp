#include "live-scenes-model.hpp"

#include <QJsonObject>

namespace fiji::model {

QList<LiveScene> LiveScenesModel::scenes() const
{
	std::lock_guard lock(mutex_);
	return scenes_;
}

int LiveScenesModel::current() const
{
	std::lock_guard lock(mutex_);
	return current_;
}

QJsonArray LiveScenesModel::toJson() const
{
	std::lock_guard lock(mutex_);
	QJsonArray out;
	for (const auto &scene : scenes_) {
		out.append(QJsonObject{{"name", scene.name}, {"uuid", scene.uuid}});
	}
	return out;
}

void LiveScenesModel::load(const QJsonArray &scenes)
{
	{
		std::lock_guard lock(mutex_);
		scenes_.clear();
		for (const auto &value : scenes) {
			const auto scene = value.toObject();
			scenes_.append({scene.value("name").toString(), scene.value("uuid").toString()});
		}
		current_ = -1;
	}
	emit changed();
}

bool LiveScenesModel::add(const QString &name, const QString &uuid)
{
	if (name.isEmpty()) {
		return false;
	}
	{
		std::lock_guard lock(mutex_);
		scenes_.append({name, uuid});
	}
	emit changed();
	return true;
}

bool LiveScenesModel::removeAt(int row)
{
	{
		std::lock_guard lock(mutex_);
		if (row < 0 || row >= scenes_.size()) {
			return false;
		}
		scenes_.removeAt(row);
		if (current_ == row) {
			current_ = -1;
		} else if (current_ > row) {
			current_--;
		}
	}
	emit changed();
	return true;
}

bool LiveScenesModel::removeNamed(const QString &name)
{
	int row = -1;
	{
		std::lock_guard lock(mutex_);
		for (int i = 0; i < scenes_.size(); i++) {
			if (scenes_.at(i).name == name) {
				row = i;
				break;
			}
		}
	}
	return !name.isEmpty() && row >= 0 && removeAt(row);
}

bool LiveScenesModel::move(int row, int delta)
{
	{
		std::lock_guard lock(mutex_);
		const int to = row + delta;
		if (row < 0 || row >= scenes_.size() || to < 0 || to >= scenes_.size() || delta == 0) {
			return false;
		}
		scenes_.move(row, to);
		if (current_ == row) {
			current_ = to;
		} else if (row < current_ && to >= current_) {
			current_--;
		} else if (row > current_ && to <= current_ && current_ >= 0) {
			current_++;
		}
	}
	emit changed();
	return true;
}

void LiveScenesModel::renamed(const QString &previous, const QString &next)
{
	bool any = false;
	{
		std::lock_guard lock(mutex_);
		for (auto &scene : scenes_) {
			if (scene.name == previous) {
				scene.name = next;
				any = true;
			}
		}
	}
	if (any) {
		emit changed();
	}
}

void LiveScenesModel::setProgram(const QString &name, const QString &uuid)
{
	{
		std::lock_guard lock(mutex_);
		int found = -1;
		bool tookName = false;
		for (int i = 0; i < scenes_.size() && found < 0; i++) {
			if (!name.isEmpty() && scenes_.at(i).name == name) {
				found = i;
			}
		}
		for (int i = 0; i < scenes_.size() && found < 0; i++) {
			if (!uuid.isEmpty() && scenes_.at(i).uuid == uuid) {
				found = i;
				tookName = scenes_.at(i).name != name;
				scenes_[i].name = name;
			}
		}
		if (found == current_ && !tookName) {
			return;
		}
		current_ = found;
	}
	emit changed();
}

} // namespace fiji::model
