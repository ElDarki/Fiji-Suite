#pragma once

// STATE layer: the life cycle of each deck key, so views (the deck pages, a future dock) can show a key that is
// held down apart from one that is running, without working it out themselves. Qt Core only.
//
//   state          press          release        run starts       run ends
//   IDLE           HELD           -              RUNNING          -
//   HELD           -              IDLE           HELD_RUNNING     -
//   RUNNING        HELD_RUNNING   -              -                IDLE
//   HELD_RUNNING   -              RUNNING        -                HELD
//
// Inputs that are not transitions from the current state change nothing.

#include <QHash>
#include <QObject>
#include <QString>

namespace fiji::state {

enum class KeyState { IDLE, HELD, RUNNING, HELD_RUNNING };
enum class KeyInput { Pressed, Released, RunStarted, RunEnded };

// The transition table.
KeyState next(KeyState from, KeyInput input);
QString keyStateName(KeyState state); // "idle", "held", "running", "held_running"

class DeckKeys : public QObject {
	Q_OBJECT

public:
	explicit DeckKeys(QObject *parent = nullptr);

	KeyState state(const QString &button) const;
	void input(const QString &button, KeyInput input);
	void forget(const QString &button); // the button was deleted
	void clear();

signals:
	void keyStateChanged(const QString &button, fiji::state::KeyState state);

private:
	QHash<QString, KeyState> states_; // only keys that are not IDLE
};

} // namespace fiji::state
