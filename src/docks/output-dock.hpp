#pragma once

#include <obs.h>
#include <obs-frontend-api.h>
#include <QCheckBox>
#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <src/utils/widgets/output-widget.hpp>
#include "../controller/output-sequencer.hpp"
#include "../obs/startable-port.hpp"

// A snapshot of one output for the multiview and remote control. `main` marks OBS's own
// stream / record / replay / virtual camera outputs; their canvas is empty.
struct OutputState {
	QString name;
	QString type;
	QString canvas;
	QString group;
	QString server;
	bool main = false;
	bool active = false;
	bool reconnecting = false;
	QDateTime startTime;
	uint64_t totalBytes = 0;
	int totalFrames = 0;
	int droppedFrames = 0;
};

// The view of the outputs. It also carries them out for the output sequencer (obs::StartableOutputs) until the
// widgets and their encoder setup move into src/obs.
class OutputDock : public QFrame, public fiji::obs::StartableOutputs {
	Q_OBJECT

private:
	int outputPlatformIconSize = 36;
	QVBoxLayout *mainLayout = nullptr;
	QLabel *mainPlatformIconLabel = nullptr;
	QLabel *mainStreamCanvasLabel = nullptr;
	QPushButton *mainStreamButton = nullptr;
	QPushButton *mainRecordButton = nullptr;
	QPushButton *mainBacktrackCheckboxButton = nullptr;
	QCheckBox *mainBacktrackCheckbox = nullptr;
	QPushButton *mainBacktrackButton = nullptr;
	QPushButton *mainVirtualCamButton = nullptr;
	QFrame *mainStreamGroup = nullptr;
	QFrame *mainRecordGroup = nullptr;
	QFrame *mainBacktrackGroup = nullptr;
	QFrame *mainVirtualCamGroup = nullptr;
	QString mainPlatformUrl;
	bool exiting = false;
	// OBS creates its own outputs after plugins load; asking for them earlier crashes inside OBS.
	bool frontendReady = false;
	bool mainStreamEnabled = true;
	bool mainRecordEnabled = true;
	bool mainBacktrackEnabled = true;
	bool mainVirtualCamEnabled = true;
	QDateTime mainStreamStartTime;
	QDateTime mainRecordStartTime;
	QDateTime mainBacktrackStartTime;
	QDateTime mainVirtualCamStartTime;

	QTimer videoCheckTimer;

	std::vector<OutputWidget *> outputWidgets;

	fiji::controller::OutputSequencer sequencer{*this};

	obs_hotkey_pair_id StartStopHotkey = OBS_INVALID_HOTKEY_PAIR_ID;
	obs_hotkey_id StartStreamHotkey = OBS_INVALID_HOTKEY_ID;
	obs_hotkey_id StartRecordHotkey = OBS_INVALID_HOTKEY_ID;

	std::function<void()> mainStreamOnStarted;
	std::function<void()> mainRecordOnStarted;
	std::function<void()> mainBacktrackOnStarted;
	std::function<void()> mainVirtualCamOnStarted;

	static void frontend_event(enum obs_frontend_event event, void *private_data);

	struct GroupRow {
		QString name;
		QFrame *row = nullptr;
		QLabel *label = nullptr;
		obs_hotkey_pair_id hotkey = OBS_INVALID_HOTKEY_PAIR_ID;
	};
	QVBoxLayout *groupsLayout = nullptr;
	// a list, so rows keep their address while they are registered as hotkey data
	std::list<GroupRow> groupRows;
	void SaveGroupHotkeys();
	void ClearGroupRows();
	void UpdateGroupStatus();
	void StartOutputs(const std::function<bool(OutputWidget *)> &filter);
	OutputWidget *widgetNamed(const QString &name) const;

private slots:
	void StartAll(bool streamOnly, bool recordOnly);
	void StopAll(bool streamOnly, bool recordOnly);

public:
	OutputDock(QWidget *parent = nullptr);
	~OutputDock();

	obs_data_array_t *GetOutputsArray();

	// fiji::obs::StartableOutputs
	bool startOutput(const QString &id, std::function<void()> onLive) override;

	void Exiting() { exiting = true; }
	void LoadSettings();
	void SaveSettings();
	bool AddChapterToOutput(const char *output_name, const char *chapter_name);
	std::vector<OutputState> GetOutputStates() const;
	QStringList GetGroups() const;
	void RebuildGroups();

public slots:
	void StartGroup(const QString &group);
	void StopGroup(const QString &group);
	void UpdateMainStreamStatus(bool active);
	void UpdateMainRecordingStatus(bool active);
	void UpdateMainBacktrackStatus(bool active);
	void UpdateMainVirtualCameraStatus(bool active);
};
