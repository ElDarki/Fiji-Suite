#pragma once

#include <obs.h>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QDateTime>
#include <functional>
#include "../../obs/output-runner.hpp"

class OutputWidget : public QFrame {
	Q_OBJECT
private:
	obs_data_t *settings = nullptr;
	int outputPlatformIconSize = 36;

	QPushButton *outputButton = nullptr;
	QPushButton *extraButton = nullptr;
	QLabel *canvasLabel = nullptr;
	fiji::obs::OutputRunner *runner = nullptr; // the output at work in OBS


	QTimer activeTimer;
	QDateTime startTime;

	obs_hotkey_pair_id StartStopHotkey = OBS_INVALID_HOTKEY_PAIR_ID;
	obs_hotkey_pair_id PauseHotkey = OBS_INVALID_HOTKEY_PAIR_ID;
	obs_hotkey_id extraHotkey = OBS_INVALID_HOTKEY_ID;
	obs_hotkey_id splitHotkey = OBS_INVALID_HOTKEY_ID;
	obs_hotkey_id chapterHotkey = OBS_INVALID_HOTKEY_ID;

	bool StartOutput(bool automated = false);
	void UpdateCanvas();
	void contextMenuEvent(QContextMenuEvent *event) override;
	void emitOutputEvent(const char *event, const char *lastError = nullptr, long long code = 0);

public:
	OutputWidget(obs_data_t *output_data, QWidget *parent = nullptr);
	~OutputWidget();

	obs_output_t *GetOutput() const { return runner->output(); }

	void CheckActive();
	void SaveSettings();
	void UpdateSettings(obs_data_t *data);
	bool AddChapter(const char *chapter_name);
	bool StartOutput(std::function<void()> onStarted);
	void StopOutput();
	bool IsStream() const;
	bool IsRecord() const;
	const char* GetOutputType() const;
	QString GetCanvasName() const;
	QString GetGroup() const;
	QString GetServer() const;
	bool IsActive() const;
	QDateTime GetStartTime() const { return startTime; }
};
