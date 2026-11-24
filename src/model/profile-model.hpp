#pragma once

// MODEL layer: Fiji Suite's settings for the current OBS profile (fiji-suite.json in the profile folder): canvases,
// outputs, which of OBS's own outputs the output dock shows, hotkeys, ... Qt Core only.
//
// The docks and dialogs used to share one obs_data_t (current_profile_config in fiji-suite.cpp) and change it in
// place. They move onto this model one part at a time: the model owns the parts that have moved (ownedKeys()), is the
// only thing that writes them, and the composition mirrors them into the old object for code that has not moved yet.

#include <QJsonArray>
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

	// Hotkey bindings, as OBS saves them (JSON arrays; "group_hotkeys" is {group: {"start": [...], "stop": [...]}}).
	// Keys: start_all_outputs_hotkey, stop_all_outputs_hotkey, start_all_streams_hotkey, start_all_recordings_hotkey,
	// multiview_over_preview_hotkey, group_hotkeys.
	static QStringList hotkeyKeys();
	QJsonValue hotkey(const QString &key) const { return hotkeys_.value(key); }
	bool setHotkey(const QString &key, const QJsonValue &value);

	// Fiji Suite's own outputs (stream, record, backtrack, virtual_cam, ffmpeg), each an object with a unique "name"
	// and its settings (see the output dialogs).
	QJsonArray outputs() const { return outputs_; }
	bool setOutputs(const QJsonArray &outputs);
	// Replaces the output with that name (an output changing its own settings: hotkeys, group, ...). False when there
	// is no such output or nothing changed.
	bool updateOutput(const QString &name, const QJsonObject &output);

	// The canvases Fiji Suite adds (and clones of them), each an object with "name", "uuid" once OBS made it, size,
	// colour, and the canvas dock's own settings (panels, projectors, transitions, studio mode, ...).
	QJsonArray canvases() const { return canvases_; }
	bool setCanvases(const QJsonArray &canvases);
	// Replaces the canvas with the same "uuid" (or, before it has one, the same "name"): a canvas dock changing its own
	// settings. False when there is no such canvas or nothing changed.
	bool updateCanvas(const QJsonObject &canvas);

signals:
	void changed();

private:
	MainOutputs main_;
	QJsonObject hotkeys_;
	QJsonArray outputs_;
	bool hasOutputs_ = false;
	QJsonArray canvases_;
	bool hasCanvases_ = false;
};

} // namespace fiji::model
