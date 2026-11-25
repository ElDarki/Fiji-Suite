#include "obs-deck-hotkeys.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <QJsonDocument>
#include <QMetaObject>
#include <QPointer>

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

ObsDeckHotkeys::~ObsDeckHotkeys()
{
	unregisterAll();
}

void ObsDeckHotkeys::pressed(void *data, obs_hotkey_id id, obs_hotkey_t *, bool down)
{
	QPointer<ObsDeckHotkeys> self = static_cast<ObsDeckHotkeys *>(data);
	QMetaObject::invokeMethod(
		self.data(),
		[self, id, down] {
			if (!self || !self->onKey) {
				return;
			}
			for (const auto &h : self->hotkeys) {
				if (h.id == id) {
					self->onKey(h.button, down);
					return;
				}
			}
		},
		Qt::QueuedConnection);
}

void ObsDeckHotkeys::registerAll(const QJsonArray &decks)
{
	unregisterAll();
	for (const auto &deckValue : decks) {
		const auto deck = deckValue.toObject();
		for (const auto &buttonValue : deck.value(QStringLiteral("buttons")).toArray()) {
			const auto button = buttonValue.toObject();
			const QString id = button.value(QStringLiteral("id")).toString();
			const QString label = QString::fromUtf8(obs_module_text("DeckHotkey"))
						      .arg(deck.value(QStringLiteral("name")).toString(),
							   button.value(QStringLiteral("text")).toString().simplified());
			Hotkey hotkey{id, obs_hotkey_register_frontend((QStringLiteral("FijiSuiteDeck") + id).toUtf8().constData(),
								      label.toUtf8().constData(), pressed, this)};
			const auto bindings = button.value(QStringLiteral("hotkey")).toArray();
			if (!bindings.isEmpty()) {
				OBSDataAutoRelease wrapper = to_obs(QJsonObject{{QStringLiteral("keys"), bindings}});
				OBSDataArrayAutoRelease keys = obs_data_get_array(wrapper, "keys");
				obs_hotkey_load(hotkey.id, keys);
			}
			hotkeys.push_back(hotkey);
		}
	}
}

void ObsDeckHotkeys::unregisterAll()
{
	for (const auto &h : hotkeys) {
		obs_hotkey_unregister(h.id);
	}
	hotkeys.clear();
}

QHash<QString, QJsonArray> ObsDeckHotkeys::bindings() const
{
	QHash<QString, QJsonArray> out;
	for (const auto &h : hotkeys) {
		OBSDataArrayAutoRelease keys = obs_hotkey_save(h.id);
		OBSDataAutoRelease wrapper = obs_data_create();
		obs_data_set_array(wrapper, "keys", keys);
		out.insert(h.button, to_json(wrapper).value(QStringLiteral("keys")).toArray());
	}
	return out;
}

} // namespace fiji::obs
