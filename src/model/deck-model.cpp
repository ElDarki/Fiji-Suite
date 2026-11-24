#include "deck-model.hpp"

#include "local-store.hpp"

#include <QSet>
#include <QUuid>

namespace fiji::model {

namespace {
QString shortId()
{
	return QUuid::createUuid().toString(QUuid::Id128).left(12);
}
} // namespace

DeckModel::DeckModel(LocalStore &store, QObject *parent) : QObject(parent), store_(store), newId_(shortId) {}

void DeckModel::load()
{
	const auto root = store_.load(QLatin1String(kStoreKey));
	decks_ = root.value(QStringLiteral("decks")).toArray();
	soundMonitoring_ = root.value(QStringLiteral("sound_monitoring")).toBool(true);
}

bool DeckModel::saveNow()
{
	return store_.save(QLatin1String(kStoreKey), QJsonObject{{QStringLiteral("version"), 1},
								  {QStringLiteral("sound_monitoring"), soundMonitoring_},
								  {QStringLiteral("decks"), decks_}});
}

QJsonArray DeckModel::triggers() const
{
	QJsonArray all;
	for (const auto &deckValue : decks_) {
		const auto deck = deckValue.toObject();
		for (const auto &buttonValue : deck.value(QStringLiteral("buttons")).toArray()) {
			const auto button = buttonValue.toObject();
			for (const auto &triggerValue : button.value(QStringLiteral("triggers")).toArray()) {
				auto trigger = triggerValue.toObject();
				trigger.insert(QStringLiteral("button"), button.value(QStringLiteral("id")));
				trigger.insert(QStringLiteral("deck"), deck.value(QStringLiteral("id")));
				all.append(trigger);
			}
		}
	}
	return all;
}

QJsonObject DeckModel::saveDeck(QJsonObject deck)
{
	QSet<QString> taken;
	const QString deckId = deck.value(QStringLiteral("id")).toString();
	for (const auto &other : decks_) {
		if (!deckId.isEmpty() && other.toObject().value(QStringLiteral("id")).toString() == deckId) {
			continue;
		}
		for (const auto &button : other.toObject().value(QStringLiteral("buttons")).toArray()) {
			taken.insert(button.toObject().value(QStringLiteral("id")).toString());
		}
	}
	if (deckId.isEmpty()) {
		deck.insert(QStringLiteral("id"), newId_());
	}
	QJsonArray buttons;
	for (const auto &value : deck.value(QStringLiteral("buttons")).toArray()) {
		auto button = value.toObject();
		QString id = button.value(QStringLiteral("id")).toString();
		if (id.isEmpty() || taken.contains(id)) {
			id = newId_();
			button.insert(QStringLiteral("id"), id);
		}
		taken.insert(id);
		buttons.append(button);
	}
	deck.insert(QStringLiteral("buttons"), buttons);

	bool replaced = false;
	for (int i = 0; i < decks_.size(); i++) {
		if (decks_.at(i).toObject().value(QStringLiteral("id")) == deck.value(QStringLiteral("id"))) {
			decks_.replace(i, deck);
			replaced = true;
			break;
		}
	}
	if (!replaced) {
		decks_.append(deck);
	}
	saveNow();
	emit decksChanged();
	return deck;
}

bool DeckModel::deleteDeck(const QString &id)
{
	for (int i = 0; i < decks_.size(); i++) {
		if (decks_.at(i).toObject().value(QStringLiteral("id")).toString() == id) {
			decks_.removeAt(i);
			saveNow();
			emit decksChanged();
			return true;
		}
	}
	return false;
}

void DeckModel::setHotkeyBindings(const QHash<QString, QJsonArray> &bindings)
{
	for (int d = 0; d < decks_.size(); d++) {
		auto deck = decks_.at(d).toObject();
		auto buttons = deck.value(QStringLiteral("buttons")).toArray();
		for (int b = 0; b < buttons.size(); b++) {
			auto button = buttons.at(b).toObject();
			const auto found = bindings.constFind(button.value(QStringLiteral("id")).toString());
			if (found != bindings.constEnd()) {
				button.insert(QStringLiteral("hotkey"), *found);
				buttons.replace(b, button);
			}
		}
		deck.insert(QStringLiteral("buttons"), buttons);
		decks_.replace(d, deck);
	}
}

} // namespace fiji::model
