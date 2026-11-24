#pragma once

// DeckSteps with libobs. Lives on the UI thread. Sounds play on output channels 40-59 of their own, so they are mixed
// like any other audio source.

#include "deck-ports.hpp"

#include <QObject>
#include <memory>

namespace fiji::obs {

class ObsDeckSteps final : public QObject, public DeckSteps {
public:
	explicit ObsDeckSteps(QObject *parent = nullptr);
	~ObsDeckSteps() override;

	bool run(const QJsonObject &step) override;
	void stopSounds(const QString &name) override;
	void setSoundMonitoring(bool on) override { monitoring_ = on; }

private:
	struct Sounds;
	bool playSound(const QJsonObject &step);

	std::unique_ptr<Sounds> sounds_;
	bool monitoring_ = true;
};

} // namespace fiji::obs
