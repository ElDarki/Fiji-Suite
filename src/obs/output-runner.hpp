#pragma once

// One of the plugin's outputs at work in OBS: builds the OBS output from its settings (stream, recording, replay
// buffer, virtual camera or an FFmpeg output), finds or makes its video and audio encoders, starts and stops it, and
// reports what OBS says about it as signals on the UI thread. The output widget (the view) shows them.
//
// Free of obs.h: OBS's types are named by their struct only. The .cpp is the only code that touches them.

#include <QObject>
#include <QString>
#include <functional>

struct obs_data;
struct obs_output;
struct obs_encoder;
struct calldata;

namespace fiji::obs {

class OutputRunner : public QObject {
	Q_OBJECT

public:
	explicit OutputRunner(QObject *parent = nullptr);
	~OutputRunner() override;

	// Builds and starts the output from its settings (any running one is stopped first). False when it could not:
	// problem() then says why, as a locale key the view can show ("NoVideoEncoder", "MainOutputNotActive",
	// "OtherOutputNotActive", "MainOutputEncoderIndexNotFound", "NoAudioEncoder", "CanvasNotFound"), "VirtualCamBusy"
	// when OBS's own virtual camera is on, or empty when there is nothing to tell.
	bool start(obs_data *settings);
	QString problem() const { return problem_; }
	// The canvas the virtual camera was started on (empty for the other kinds).
	QString virtualCamCanvas() const { return virtualCamCanvas_; }

	void stop();           // stops it (gently when it has a stream delay)
	void release();        // lets go of the OBS output
	bool active() const;
	obs_output *output() const; // for the view's pause, split and chapter controls

	// Runs once, on OBS's thread, when the output started or gave up starting (the output sequencer's callback).
	void setOnStarted(std::function<void()> onStarted);

signals:
	void started();
	void stopped(const QString &lastError, long long code, bool hadOutput); // hadOutput: it was this runner's
	void reconnecting();
	void reconnected();
	void replaySaved();

private:
	obs_encoder *videoEncoder(obs_data *settings, bool advanced, bool is_record, const char *output_name);
	static void output_start(void *data, calldata *cd);
	static void output_stop(void *data, calldata *cd);
	static void output_reconnect(void *data, calldata *cd);
	static void output_reconnect_success(void *data, calldata *cd);
	static void replay_saved(void *data, calldata *cd);

	obs_output *output_ = nullptr;
	QString name_;
	QString problem_;
	QString virtualCamCanvas_;
	bool isBacktrack_ = false;
	std::function<void()> onStarted_;
};

} // namespace fiji::obs
