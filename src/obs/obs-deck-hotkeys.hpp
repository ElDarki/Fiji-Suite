#pragma once

// DeckHotkeys with OBS's frontend hotkeys: one per deck button, named "FijiSuiteDeck<button id>", shown in
// Settings -> Hotkeys as "<deck>: <key text>". Lives on the UI thread; OBS calls the hotkeys on its own thread and
// they are passed on to the UI thread.

#include "deck-port.hpp"

#include <obs.hpp>
#include <QObject>
#include <vector>

namespace fiji::obs {

class ObsDeckHotkeys final : public QObject, public DeckHotkeys {
	Q_OBJECT

public:
	using QObject::QObject;
	~ObsDeckHotkeys() override;

	void registerAll(const QJsonArray &decks) override;
	void unregisterAll() override;
	QHash<QString, QJsonArray> bindings() const override;

private:
	struct Hotkey {
		QString button;
		obs_hotkey_id id;
	};
	static void pressed(void *data, obs_hotkey_id id, obs_hotkey_t *, bool down);

	std::vector<Hotkey> hotkeys;
};

} // namespace fiji::obs
