#pragma once

// MODEL layer: the game and window captures the Capture dock lists, each with the window it has hooked. Keyed by the
// source's uuid. Qt Core only; changed on the UI thread.

#include <QList>
#include <QObject>
#include <QString>
#include <QVariant>
#include <functional>

namespace fiji::model {

struct Capture {
	QString key;    // the source's uuid
	QString name;   // the source's name
	QString hooked; // "[game.exe]: Window title", or empty while nothing is hooked
};

// One window a capture can hook, as its settings menu offers it.
struct WindowChoice {
	QString label;  // what the menu shows
	QVariant value; // the source's "window" setting (a string or a number, as the source defines it)
};

// The source kinds the dock lists (OBS's unversioned ids).
bool isCaptureKind(const QString &sourceKind);
// What the dock shows for a hooked window.
QString hookedText(const QString &executable, const QString &title);

class Captures : public QObject {
	Q_OBJECT

public:
	explicit Captures(QObject *parent = nullptr);

	QList<Capture> entries() const { return entries_; }
	int indexOf(const QString &key) const;

	void add(const QString &key, const QString &name); // ignored when it is already listed
	void remove(const QString &key);
	void renamed(const QString &key, const QString &name);
	void hooked(const QString &key, const QString &executable, const QString &title);
	void unhooked(const QString &key);

signals:
	void added(int index);
	void removed(int index);
	void changed(int index);

private:
	void update(const QString &key, const std::function<bool(Capture &)> &change);

	QList<Capture> entries_;
};

} // namespace fiji::model
