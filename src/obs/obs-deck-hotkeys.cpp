#include "obs-deck-hotkeys.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <obs.hpp>

#include <QJsonDocument>
#include <QMetaObject>

namespace fiji::obs {

namespace {

OBSDataAutoRelease to_obs(const QJsonObject &object)
{
	return obs_data_create_from_json(QJsonDocument(object).toJson(QJsonDocument::Compact).constData());
}

QJsonObject to_json(obs_data_t *data)
{
	return data ? QJsonDocument::fromJson(QByteArray(obs_data_get_json(data))).object() : QJsonObject();
}

} // namespace

void ObsDeckHotkeys::bind(const QJsonArray &decks, KeyFn onKey)
{
	clear();
	onKey_ = std::move(onKey);
	for (const auto &deckValue : decks) {
		const auto deck = deckValue.toObject();
		for (const auto &buttonValue : deck.value(QStringLiteral("buttons")).toArray()) {
			const auto button = buttonValue.toObject();
			const QString id = button.value(QStringLiteral("id")).toString();
			const QString label = QString::fromUtf8(obs_module_text("DeckHotkey"))
						      .arg(deck.value(QStringLiteral("name")).toString(),
							   button.value(QStringLiteral("text")).toString().simplified());
			const obs_hotkey_id hotkey = obs_hotkey_register_frontend(
				(QStringLiteral("FijiSuiteDeck") + id).toUtf8().constData(), label.toUtf8().constData(),
				[](void *data, obs_hotkey_id fired, obs_hotkey_t *, bool pressed) {
					auto self = static_cast<ObsDeckHotkeys *>(data);
					// OBS's hotkey thread: hand the key to the controller's thread, which finds the button by id
					QMetaObject::invokeMethod(
						self->context_,
						[self, fired, pressed] {
							for (const auto &h : self->hotkeys_) {
								if (h.id == fired && self->onKey_) {
									self->onKey_(h.button, pressed);
									return;
								}
							}
						},
						Qt::QueuedConnection);
				},
				this);
			const auto saved = button.value(QStringLiteral("hotkey")).toArray();
			if (!saved.isEmpty()) {
				OBSDataAutoRelease wrapper = to_obs(QJsonObject{{QStringLiteral("keys"), saved}});
				OBSDataArrayAutoRelease keys = obs_data_get_array(wrapper, "keys");
				obs_hotkey_load(hotkey, keys);
			}
			hotkeys_.push_back({id, hotkey});
		}
	}
}

QHash<QString, QJsonArray> ObsDeckHotkeys::bindings() const
{
	QHash<QString, QJsonArray> out;
	for (const auto &h : hotkeys_) {
		OBSDataArrayAutoRelease keys = obs_hotkey_save(h.id);
		OBSDataAutoRelease wrapper = obs_data_create();
		obs_data_set_array(wrapper, "keys", keys);
		out.insert(h.button, to_json(wrapper).value(QStringLiteral("keys")).toArray());
	}
	return out;
}

void ObsDeckHotkeys::clear()
{
	for (const auto &h : hotkeys_) {
		obs_hotkey_unregister(h.id);
	}
	hotkeys_.clear();
}

} // namespace fiji::obs
