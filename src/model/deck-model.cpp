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

QString idOf(const QJsonValue &value)
{
	return value.toObject().value(QStringLiteral("id")).toString();
}

} // namespace

DeckModel::DeckModel(LocalStore &store, QObject *parent) : QObject(parent), store_(store) {}

void DeckModel::load()
{
	const auto root = store_.load(QLatin1String(kStoreKey));
	decks_ = root.value(QStringLiteral("decks")).toArray();
	soundMonitoring_ = root.value(QStringLiteral("sound_monitoring")).toBool(true);
}

bool DeckModel::save()
{
	return store_.save(QLatin1String(kStoreKey), QJsonObject{{QStringLiteral("version"), 1},
								 {QStringLiteral("sound_monitoring"), soundMonitoring_},
								 {QStringLiteral("decks"), decks_}});
}

bool DeckModel::hasButton(const QString &id) const
{
	for (const auto &deck : decks_) {
		for (const auto &button : deck.toObject().value(QStringLiteral("buttons")).toArray()) {
			if (idOf(button) == id) {
				return true;
			}
		}
	}
	return false;
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

QJsonObject DeckModel::saveDeck(QJsonObject deck, const Bindings &bindings)
{
	// button ids are what presses, triggers and other buttons refer to: unique across all decks
	QSet<QString> taken;
	const QString deckId = deck.value(QStringLiteral("id")).toString();
	for (const auto &other : decks_) {
		if (!deckId.isEmpty() && idOf(other) == deckId) {
			continue;
		}
		for (const auto &button : other.toObject().value(QStringLiteral("buttons")).toArray()) {
			taken.insert(idOf(button));
		}
	}
	if (deckId.isEmpty()) {
		deck.insert(QStringLiteral("id"), shortId());
	}
	QJsonArray buttons;
	for (const auto &value : deck.value(QStringLiteral("buttons")).toArray()) {
		auto button = value.toObject();
		QString id = button.value(QStringLiteral("id")).toString();
		if (id.isEmpty() || taken.contains(id)) {
			id = shortId();
			button.insert(QStringLiteral("id"), id);
		}
		taken.insert(id);
		buttons.append(button);
	}
	deck.insert(QStringLiteral("buttons"), buttons);

	bool replaced = false;
	for (int i = 0; i < decks_.size(); i++) {
		if (idOf(decks_.at(i)) == deck.value(QStringLiteral("id")).toString()) {
			decks_.replace(i, deck);
			replaced = true;
			break;
		}
	}
	if (!replaced) {
		decks_.append(deck);
	}
	setBindings(bindings);
	save();
	emit decksChanged();
	return deck;
}

bool DeckModel::deleteDeck(const QString &id, const Bindings &bindings)
{
	for (int i = 0; i < decks_.size(); i++) {
		if (idOf(decks_.at(i)) == id) {
			decks_.removeAt(i);
			setBindings(bindings);
			save();
			emit decksChanged();
			return true;
		}
	}
	return false;
}

void DeckModel::setBindings(const Bindings &bindings)
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
