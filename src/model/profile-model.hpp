#pragma once

// MODEL layer: Fiji Suite's settings for the current OBS profile (fiji-suite.json in the profile folder): canvases,
// outputs, which of OBS's own outputs the output dock shows, hotkeys, ... Qt Core only.
//
// The docks and dialogs used to share one obs_data_t (current_profile_config in fiji-suite.cpp) and change it in
// place. They move onto this model one part at a time: the model owns the parts that have moved (ownedKeys()), is the
// only thing that writes them, and the composition mirrors them into the old object for code that has not moved yet.

#include <QJsonObject>
#include <QObject>
#include <QStringList>
#include <utility>

namespace fiji::model {

// Which of OBS's own outputs the output dock shows.
struct MainOutputs {
	bool stream = true;
	bool record = true;
	bool backtrack = true;
	bool virtualCam = true;

	bool operator==(const MainOutputs &other) const
	{
		return stream == other.stream && record == other.record && backtrack == other.backtrack &&
		       virtualCam == other.virtualCam;
	}
	bool operator!=(const MainOutputs &other) const { return !(*this == other); }
};

// What a profile starts with. `loaded` is the file's content (empty when there was none); `recordPath` is where OBS
// records for this profile (the default Vertical Backtrack saves there). Returns the document and whether the default
// Vertical canvas was just created.
std::pair<QJsonObject, bool> withFirstRunDefaults(QJsonObject loaded, bool fileExisted, const QString &recordPath);

class ProfileModel : public QObject {
	Q_OBJECT

public:
	explicit ProfileModel(QObject *parent = nullptr) : QObject(parent) {}

	// The whole document of the profile just loaded (after withFirstRunDefaults).
	void load(const QJsonObject &document);

	// The keys this model owns, and their values, to mirror into the old settings object and write to the file.
	static QStringList ownedKeys();
	QJsonObject owned() const;

	MainOutputs mainOutputs() const { return main_; }
	// True when something changed (then `changed` was emitted).
	bool setMainOutputs(const MainOutputs &outputs);

signals:
	void changed();

private:
	MainOutputs main_;
};

} // namespace fiji::model
