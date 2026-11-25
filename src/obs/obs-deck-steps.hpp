#pragma once

// DeckSteps with libobs: scenes (OBS's own and the plugin's canvases), sources, filters, text, mute, sounds on
// their own output channels, and requests of the plugin's request table. Lives on the UI thread.

#include "deck-port.hpp"

#include <obs.hpp>
#include <QObject>
#include <vector>

namespace fiji::obs {

class ObsDeckSteps final : public QObject, public DeckSteps {
	Q_OBJECT

public:
	using QObject::QObject;
	~ObsDeckSteps() override;

	void run(const QJsonObject &step) override;
	void stopSounds(const QString &name) override;
	void setSoundMonitoring(bool on) override;

private:
	struct Sound {
		int channel;
		OBSSource source;
		QString name;
	};

	void playSound(const QJsonObject &step);
	static void soundEnded(void *data, calldata_t *calldata);

	bool soundMonitoring = true;
	std::vector<Sound> sounds;
};

} // namespace fiji::obs
