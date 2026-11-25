#include "deck-keys.hpp"

namespace fiji::state {

KeyState next(KeyState from, KeyInput input)
{
	switch (from) {
	case KeyState::IDLE:
		return input == KeyInput::Pressed ? KeyState::HELD : input == KeyInput::RunStarted ? KeyState::RUNNING : from;
	case KeyState::HELD:
		return input == KeyInput::Released     ? KeyState::IDLE
		       : input == KeyInput::RunStarted ? KeyState::HELD_RUNNING
						       : from;
	case KeyState::RUNNING:
		return input == KeyInput::Pressed ? KeyState::HELD_RUNNING : input == KeyInput::RunEnded ? KeyState::IDLE : from;
	case KeyState::HELD_RUNNING:
		return input == KeyInput::Released ? KeyState::RUNNING : input == KeyInput::RunEnded ? KeyState::HELD : from;
	}
	return from;
}

QString keyStateName(KeyState state)
{
	switch (state) {
	case KeyState::IDLE:
		return QStringLiteral("idle");
	case KeyState::HELD:
		return QStringLiteral("held");
	case KeyState::RUNNING:
		return QStringLiteral("running");
	case KeyState::HELD_RUNNING:
		return QStringLiteral("held_running");
	}
	return QStringLiteral("idle");
}

DeckKeys::DeckKeys(QObject *parent) : QObject(parent) {}

KeyState DeckKeys::state(const QString &button) const
{
	return states_.value(button, KeyState::IDLE);
}

void DeckKeys::input(const QString &button, KeyInput input)
{
	const KeyState before = state(button);
	const KeyState after = next(before, input);
	if (after == before) {
		return;
	}
	if (after == KeyState::IDLE) {
		states_.remove(button);
	} else {
		states_.insert(button, after);
	}
	emit keyStateChanged(button, after);
}

void DeckKeys::forget(const QString &button)
{
	if (states_.remove(button)) {
		emit keyStateChanged(button, KeyState::IDLE);
	}
}

void DeckKeys::clear()
{
	const auto buttons = states_.keys();
	states_.clear();
	for (const auto &button : buttons) {
		emit keyStateChanged(button, KeyState::IDLE);
	}
}

} // namespace fiji::state
