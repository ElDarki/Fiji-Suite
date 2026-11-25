#include "captures.hpp"

#include <utility>

namespace fiji::model {

bool isCaptureKind(const QString &sourceKind)
{
	return sourceKind == QStringLiteral("game_capture") || sourceKind == QStringLiteral("window_capture");
}

QString hookedText(const QString &executable, const QString &title)
{
	QString text;
	if (!executable.isEmpty()) {
		text += "[" + executable + "]: ";
	}
	text += title;
	return text;
}

Captures::Captures(QObject *parent) : QObject(parent) {}

int Captures::indexOf(const QString &key) const
{
	for (int i = 0; i < entries_.size(); i++) {
		if (entries_[i].key == key) {
			return i;
		}
	}
	return -1;
}

void Captures::add(const QString &key, const QString &name)
{
	if (key.isEmpty() || indexOf(key) >= 0) {
		return;
	}
	entries_.append({key, name, QString()});
	emit added(static_cast<int>(entries_.size()) - 1);
}

void Captures::remove(const QString &key)
{
	const int index = indexOf(key);
	if (index < 0) {
		return;
	}
	entries_.removeAt(index);
	emit removed(index);
}

void Captures::update(const QString &key, const std::function<bool(Capture &)> &change)
{
	const int index = indexOf(key);
	if (index >= 0 && change(entries_[index])) {
		emit changed(index);
	}
}

void Captures::renamed(const QString &key, const QString &name)
{
	update(key, [&](Capture &c) { return std::exchange(c.name, name) != name; });
}

void Captures::hooked(const QString &key, const QString &executable, const QString &title)
{
	const QString text = hookedText(executable, title);
	update(key, [&](Capture &c) { return std::exchange(c.hooked, text) != text; });
}

void Captures::unhooked(const QString &key)
{
	update(key, [](Capture &c) { return std::exchange(c.hooked, QString()) != QString(); });
}

} // namespace fiji::model
