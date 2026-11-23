#pragma once

// MODEL layer: the decks (pages of buttons, their steps and triggers), local-first through LocalStore.
// Views observe the signals; only controllers call the mutators. Scaffold: declarations only.

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QString>

namespace fiji::model {

class LocalStore;

class DeckModel : public QObject {
	Q_OBJECT

public:
	explicit DeckModel(LocalStore &store, QObject *parent = nullptr);

	QJsonArray decks() const;
	QJsonObject deck(const QString &id) const;

	// Mutators (controllers only). Each saves through the store before emitting.
	QJsonObject saveDeck(const QJsonObject &deck);
	bool deleteDeck(const QString &id);

signals:
	void decksChanged();
	void buttonStateChanged(const QString &button, bool running);
};

} // namespace fiji::model
