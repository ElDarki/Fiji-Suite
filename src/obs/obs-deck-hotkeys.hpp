#pragma once

// DeckHotkeys with OBS's frontend hotkeys. Hotkeys fire on OBS's hotkey thread; the key goes to `context`'s thread.

#include "deck-ports.hpp"

#include <QObject>
#include <vector>

namespace fiji::obs {

class ObsDeckHotkeys final : public DeckHotkeys {
public:
	explicit ObsDeckHotkeys(QObject *context) : context_(context) {}
	~ObsDeckHotkeys() override { clear(); }

	void bind(const QJsonArray &decks, KeyFn onKey) override;
	QHash<QString, QJsonArray> bindings() const override;
	void clear() override;

private:
	struct Hotkey {
		QString button;
		size_t id; // obs_hotkey_id
	};

	QObject *context_;
	KeyFn onKey_;
	std::vector<Hotkey> hotkeys_;
};

} // namespace fiji::obs
