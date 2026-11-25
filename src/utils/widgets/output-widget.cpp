#include "output-widget.hpp"
#include <obs.hpp>
#include <obs-frontend-api.h>
#include <obs-module.h>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMainWindow>
#include <QMessageBox>
#include <QRegularExpression>
#include <QTime>
#include <src/utils/color.hpp>
#include <src/utils/icon.hpp>
#include <QContextMenuEvent>
#include <QMenu>
#include <src/dialogs/name-dialog.hpp>
#include <src/docks/output-dock.hpp>
#include <src/utils/obs-websocket-api.h>
#include "../fiji-events.hpp"
#include <util/config-file.h>
#include <util/platform.h>

extern obs_data_t *current_profile_config;
extern bool isTwitchServer(QString outputServer);

OutputWidget::OutputWidget(obs_data_t *output_data, QWidget *parent) : QFrame(parent), settings(output_data)
{
	obs_data_addref(settings);
	runner = new fiji::obs::OutputRunner(this);
	connect(runner, &fiji::obs::OutputRunner::started, this, [this] {
		if (!outputButton->isChecked()) {
			outputButton->setChecked(true);
		}
	});
	connect(runner, &fiji::obs::OutputRunner::stopped, this,
		[this](const QString &lastError, long long code, bool hadOutput) {
			if (outputButton->isChecked()) {
				outputButton->setChecked(false);
			}
			if (hadOutput) {
				emitOutputEvent("stop_output", lastError.toUtf8().constData(), code);
			}
		});
	connect(runner, &fiji::obs::OutputRunner::reconnecting, this, [this] { emitOutputEvent("output_reconnecting"); });
	connect(runner, &fiji::obs::OutputRunner::reconnected, this, [this] { emitOutputEvent("output_reconnected"); });
	connect(runner, &fiji::obs::OutputRunner::replaySaved, this, [this] {
		startTime = QDateTime::currentDateTime();
		proc_handler_t *ph = runner->output() ? obs_output_get_proc_handler(runner->output()) : nullptr;
		if (!ph) {
			return;
		}
		QString path;
		calldata_t cd = {0};
		if (proc_handler_call(ph, "get_last_replay", &cd)) {
			const char *p = calldata_string(&cd, "path");
			if (p) {
				path = QString::fromUtf8(p);
			}
		}
		calldata_free(&cd);
		if (!path.isEmpty()) {
			if (const auto main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window())) {
				QMetaObject::invokeMethod(main_window, "RecordingFileChanged", Q_ARG(QString, path));
			}
		}
	});
	auto nameChars = obs_data_get_string(settings, "name");
	auto name = QString::fromUtf8(nameChars);

	setObjectName(name);

	auto outputLayout = new QHBoxLayout;
	outputLayout->setContentsMargins(5, 0, 5, 0);
	outputLayout->setSpacing(0);

	outputButton = new QPushButton;
	outputButton->setMinimumHeight(30);
	outputButton->setObjectName(QStringLiteral("canvasOutput"));

	auto output_type = obs_data_get_string(settings, "type");
	if (output_type[0] == '\0' || strcmp(output_type, "stream") == 0) {
		auto endpoint = QString::fromUtf8(obs_data_get_string(settings, "stream_server"));
		auto platformIconLabel = new QLabel;
		auto platformIcon = getPlatformIconFromEndpoint(endpoint);

		platformIconLabel->setPixmap(platformIcon.pixmap(outputPlatformIconSize, outputPlatformIconSize));

		outputLayout->addWidget(platformIconLabel);
	}
	outputLayout->addWidget(new QLabel(name), 1);

	if (strcmp(output_type, "record") == 0) {
		outputButton->setIcon(create2StateIcon(":/fiji/media/recording.svg", ":/fiji/media/record.svg"));
		outputButton->setToolTip(QString::fromUtf8(obs_module_text("Record")));

		std::string splitName = "FijiSuiteSplit";
		splitName += nameChars;
		std::string splitDescription = obs_frontend_get_locale_string("Basic.Main.SplitFile");
		splitDescription = splitDescription + " " + nameChars;

		splitHotkey = obs_hotkey_register_frontend(
			splitName.c_str(), splitDescription.c_str(),
			[](void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed) {
				UNUSED_PARAMETER(id);
				UNUSED_PARAMETER(hotkey);
				if (!pressed) {
					return;
				}
				auto this_ = (OutputWidget *)data;
				if (!this_->runner->output()) {
					return;
				}
				calldata_t cd = {0};
				proc_handler_t *ph = obs_output_get_proc_handler(this_->runner->output());
				proc_handler_call(ph, "split", &cd);
				calldata_free(&cd);
			},
			this);

		auto split_hotkey = obs_data_get_array(settings, "split_hotkey");
		obs_hotkey_load(splitHotkey, split_hotkey);
		obs_data_array_release(split_hotkey);

		std::string chapterName = "FijiSuiteChapter";
		chapterName += nameChars;
		std::string chapterDescription = obs_frontend_get_locale_string("Basic.Main.AddChapterMarker");
		chapterDescription = chapterDescription + " " + nameChars;

		chapterHotkey = obs_hotkey_register_frontend(
			chapterName.c_str(), chapterDescription.c_str(),
			[](void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed) {
				UNUSED_PARAMETER(id);
				UNUSED_PARAMETER(hotkey);
				if (!pressed) {
					return;
				}
				auto this_ = (OutputWidget *)data;
				if (!this_->runner->output()) {
					return;
				}
				calldata_t cd = {0};
				proc_handler_t *ph = obs_output_get_proc_handler(this_->runner->output());
				proc_handler_call(ph, "add_chapter", &cd);
				calldata_free(&cd);
			},
			this);

		auto chapter_hotkey = obs_data_get_array(settings, "chapter_hotkey");
		obs_hotkey_load(chapterHotkey, chapter_hotkey);
		obs_data_array_release(chapter_hotkey);

		std::string pauseName = "FijiSuitePause";
		pauseName += nameChars;
		std::string unpauseName = "FijiSuiteUnpause";
		unpauseName += nameChars;

		std::string pauseDescription = obs_frontend_get_locale_string("Basic.Main.PauseRecording");
		pauseDescription = pauseDescription + " " + nameChars;
		std::string unpauseDescription = obs_frontend_get_locale_string("Basic.Main.UnpauseRecording");
		unpauseDescription = unpauseDescription + " " + nameChars;

		PauseHotkey = obs_hotkey_pair_register_frontend(
			pauseName.c_str(), pauseDescription.c_str(), unpauseName.c_str(), unpauseDescription.c_str(),
			[](void *data, obs_hotkey_pair_id id, obs_hotkey_t *hotkey, bool pressed) {
				UNUSED_PARAMETER(id);
				UNUSED_PARAMETER(hotkey);
				if (!pressed) {
					return false;
				}
				auto this_ = (OutputWidget *)data;
				if (this_->runner->output() && !obs_output_paused(this_->runner->output())) {
					return obs_output_pause(this_->runner->output(), true);
				}
				return false;
			},
			[](void *data, obs_hotkey_pair_id id, obs_hotkey_t *hotkey, bool pressed) {
				UNUSED_PARAMETER(id);
				UNUSED_PARAMETER(hotkey);
				if (!pressed) {
					return false;
				}
				auto this_ = (OutputWidget *)data;
				if (this_->runner->output() && obs_output_paused(this_->runner->output())) {
					return obs_output_pause(this_->runner->output(), false);
				}
				return false;
			},
			this, this);

		auto pause_hotkey = obs_data_get_array(settings, "pause_hotkey");
		auto unpause_hotkey = obs_data_get_array(settings, "unpause_hotkey");
		obs_hotkey_pair_load(PauseHotkey, pause_hotkey, unpause_hotkey);
		obs_data_array_release(pause_hotkey);
		obs_data_array_release(unpause_hotkey);

	} else if (strcmp(output_type, "backtrack") == 0) {
		outputButton->setStyleSheet(QString::fromUtf8(
			"QPushButton{ border-top-right-radius: 0; border-bottom-right-radius: 0; width: 32px; padding-left: 0px; padding-right: 0px;}"));

		auto replayEnable = new QCheckBox;
		auto testl = new QHBoxLayout;
		outputButton->setLayout(testl);
		testl->addWidget(replayEnable);
		outputButton->setToolTip(QString::fromUtf8(obs_module_text("Backtrack")));

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
		connect(replayEnable, &QCheckBox::checkStateChanged, [this, replayEnable] {
#else
		connect(replayEnable, &QCheckBox::stateChanged, [this, replayEnable] {
#endif
			if (replayEnable->isChecked() != outputButton->isChecked()) {
				outputButton->click();
			}
		});

		extraButton = new QPushButton;
		extraButton->setCheckable(true);
		extraButton->setIcon(create2StateIcon(":/fiji/media/backtrack_on.svg", ":/fiji/media/backtrack_off.svg"));
		extraButton->setStyleSheet(QString::fromUtf8(
			"QPushButton{min-width: 32px; padding-left: 0px; padding-right: 0px; border-top-left-radius: 0; border-bottom-left-radius: 0;}"));
		extraButton->setToolTip(QString::fromUtf8(obs_module_text("SaveBacktrack")));

		connect(extraButton, &QPushButton::clicked, [this] {
			calldata_t cd = {0};
			proc_handler_t *ph = obs_output_get_proc_handler(this->runner->output());
			proc_handler_call(ph, "save", &cd);
			calldata_free(&cd);
		});

		connect(outputButton, &QPushButton::toggled, [this, replayEnable] {
			bool enabled = outputButton->isChecked();
			replayEnable->setChecked(enabled);
			extraButton->setChecked(enabled);
		});

		std::string ebName = "FijiSuiteSaveBacktrack";
		ebName += nameChars;
		std::string ebDescription = obs_module_text("SaveBacktrack");
		ebDescription = ebDescription + " " + nameChars;

		extraHotkey = obs_hotkey_register_frontend(
			ebName.c_str(), ebDescription.c_str(),
			[](void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed) {
				UNUSED_PARAMETER(id);
				UNUSED_PARAMETER(hotkey);
				if (!pressed) {
					return;
				}
				auto this_ = (OutputWidget *)data;
				QMetaObject::invokeMethod(this_->extraButton, "click");
			},
			this);

		auto extra_hotkey = obs_data_get_array(settings, "extra_hotkey");
		obs_hotkey_load(extraHotkey, extra_hotkey);
		obs_data_array_release(extra_hotkey);

	} else if (strcmp(output_type, "virtual_cam") == 0) {
		outputButton->setIcon(create2StateIcon(":/fiji/media/virtual_cam_on.svg", ":/fiji/media/virtual_cam_off.svg"));
		outputButton->setToolTip(QString::fromUtf8(obs_module_text("VirtualCamera")));
	} else if (strcmp(output_type, "ffmpeg") == 0) {
		outputButton->setIcon(create2StateIcon(":/fiji/media/ffmpeg_on.svg", ":/fiji/media/ffmpeg_off.svg"));
		outputButton->setToolTip(QString::fromUtf8(obs_module_text("Ffmpeg")));
	} else {
		outputButton->setIcon(create2StateIcon(":/fiji/media/streaming.svg", ":/fiji/media/stream.svg"));
		outputButton->setToolTip(QString::fromUtf8(obs_module_text("Stream")));
	}
	outputButton->setCheckable(true);
	outputButton->setChecked(false);

	connect(outputButton, &QPushButton::clicked, [this]() {
		auto output_type = obs_data_get_string(settings, "type");
		if (outputButton->isChecked()) {
			blog(LOG_INFO, "[Fiji Suite] start %s output clicked '%s'", output_type,
			     obs_data_get_string(settings, "name"));
			if (!StartOutput()) {
				outputButton->setChecked(false);
			}
		} else {
			bool stop = true;

			if (output_type[0] == '\0' || strcmp(output_type, "stream") == 0) {
				bool warnBeforeStreamStop =
					config_get_bool(obs_frontend_get_user_config(), "BasicWindow", "WarnBeforeStoppingStream");
				if (warnBeforeStreamStop && isVisible()) {
					auto button = QMessageBox::question(
						this, QString::fromUtf8(obs_frontend_get_locale_string("ConfirmStop.Title")),
						QString::fromUtf8(obs_frontend_get_locale_string("ConfirmStop.Text")),
						QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
					if (button == QMessageBox::No) {
						stop = false;
					}
				}
			}
			if (stop) {
				blog(LOG_INFO, "[Fiji Suite] stop %s output clicked '%s'", output_type,
				     obs_data_get_string(settings, "name"));

				obs_queue_task(
					OBS_TASK_GRAPHICS, [](void *param) { obs_output_stop((obs_output_t *)param); }, runner->output(),
					false);

			} else {
				outputButton->setChecked(true);
			}
		}
	});

	setProperty("class", "output-frame");

	canvasLabel = new QLabel;
	UpdateCanvas();
	outputLayout->addWidget(canvasLabel, 1);
	if (extraButton) {
		auto l3 = new QHBoxLayout;
		l3->setContentsMargins(0, 0, 0, 0);
		l3->setSpacing(0);
		l3->addWidget(outputButton);
		l3->addWidget(extraButton);
		outputLayout->addLayout(l3);
	} else {
		outputLayout->addWidget(outputButton);
	}

	setLayout(outputLayout);

	std::string startName = "FijiSuiteStartOutput";
	startName += nameChars;
	std::string startDescription = obs_module_text("StartHotkey");
	startDescription = startDescription + " " + nameChars;
	std::string stopName = "FijiSuiteStopOutput";
	stopName += nameChars;
	std::string stopDescription = obs_module_text("StopHotkey");
	stopDescription = stopDescription + " " + nameChars;

	StartStopHotkey = obs_hotkey_pair_register_frontend(
		startName.c_str(), startDescription.c_str(), stopName.c_str(), stopDescription.c_str(),
		[](void *data, obs_hotkey_pair_id id, obs_hotkey_t *hotkey, bool pressed) {
			UNUSED_PARAMETER(id);
			UNUSED_PARAMETER(hotkey);
			if (!pressed) {
				return false;
			}
			auto this_ = (OutputWidget *)data;
			if (!this_->runner->output() || !obs_output_active(this_->runner->output())) {
				QMetaObject::invokeMethod(this_->outputButton, "click");
				return true;
			}
			return false;
		},
		[](void *data, obs_hotkey_pair_id id, obs_hotkey_t *hotkey, bool pressed) {
			UNUSED_PARAMETER(id);
			UNUSED_PARAMETER(hotkey);
			if (!pressed) {
				return false;
			}
			auto this_ = (OutputWidget *)data;
			if (this_->runner->output() && obs_output_active(this_->runner->output())) {
				QMetaObject::invokeMethod(this_->outputButton, "click");
				return true;
			}
			return false;
		},
		this, this);

	auto start_hotkey = obs_data_get_array(output_data, "start_hotkey");
	auto stop_hotkey = obs_data_get_array(output_data, "stop_hotkey");
	obs_hotkey_pair_load(StartStopHotkey, start_hotkey, stop_hotkey);
	obs_data_array_release(start_hotkey);
	obs_data_array_release(stop_hotkey);

	connect(&activeTimer, &QTimer::timeout, this, [this] {
		auto t = QTime::fromMSecsSinceStartOfDay(startTime.msecsTo(QDateTime::currentDateTime()));
		(extraButton ? extraButton : outputButton)->setText(t.toString(t.hour() ? "hh:mm:ss" : "mm:ss"));
	});
}

OutputWidget::~OutputWidget()
{
	if (StartStopHotkey != OBS_INVALID_HOTKEY_PAIR_ID) {
		obs_hotkey_pair_unregister(StartStopHotkey);
	}
	if (PauseHotkey != OBS_INVALID_HOTKEY_PAIR_ID) {
		obs_hotkey_pair_unregister(PauseHotkey);
	}
	if (extraHotkey != OBS_INVALID_HOTKEY_ID) {
		obs_hotkey_unregister(extraHotkey);
	}
	if (splitHotkey != OBS_INVALID_HOTKEY_ID) {
		obs_hotkey_unregister(splitHotkey);
	}
	if (chapterHotkey != OBS_INVALID_HOTKEY_ID) {
		obs_hotkey_unregister(chapterHotkey);
	}
	runner->release();
	obs_data_release(settings);
}

bool OutputWidget::StartOutput(bool automated)
{
	if (!settings) {
		return false;
	}

	auto output_type = obs_data_get_string(settings, "type");
	if (!automated && (output_type[0] == '\0' || strcmp(output_type, "stream") == 0)) {

		bool warnBeforeStreamStart =
			config_get_bool(obs_frontend_get_user_config(), "BasicWindow", "WarnBeforeStartingStream");
		if (warnBeforeStreamStart && isVisible()) {
			auto button = QMessageBox::question(this,
							    QString::fromUtf8(obs_frontend_get_locale_string("ConfirmStart.Title")),
							    QString::fromUtf8(obs_frontend_get_locale_string("ConfirmStart.Text")),
							    QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
			if (button == QMessageBox::No) {
				return false;
			}
		}
	}

	if (!runner->start(settings)) {
		const QString problem = runner->problem();
		if (problem == QStringLiteral("VirtualCamBusy")) {
			outputButton->setChecked(false); // OBS's own virtual camera is on
		} else if (!automated && !problem.isEmpty()) {
			const auto text = QString::fromUtf8(obs_module_text(problem.toUtf8().constData()));
			QMessageBox::warning(this, text, text);
		}
		return false;
	}
	emitOutputEvent("start_output");
	return true;
}

void OutputWidget::CheckActive()
{
	bool active = obs_output_active(runner->output());
	if (outputButton->isChecked() != active) {
		outputButton->setChecked(active);
	}
	if (activeTimer.isActive() != active) {
		if (active) {
			startTime = QDateTime::currentDateTime();
			activeTimer.start(500);
		} else {
			activeTimer.stop();
			if (extraButton) {
				extraButton->setText("");
			} else {
				outputButton->setText("");
			}
		}
	}
}

void OutputWidget::SaveSettings()
{
	if (StartStopHotkey != OBS_INVALID_HOTKEY_PAIR_ID) {
		obs_data_array_t *start_hotkey = nullptr;
		obs_data_array_t *stop_hotkey = nullptr;
		obs_hotkey_pair_save(StartStopHotkey, &start_hotkey, &stop_hotkey);
		obs_data_set_array(settings, "start_hotkey", start_hotkey);
		obs_data_set_array(settings, "stop_hotkey", stop_hotkey);
		obs_data_array_release(start_hotkey);
		obs_data_array_release(stop_hotkey);
	}
	if (PauseHotkey != OBS_INVALID_HOTKEY_PAIR_ID) {
		obs_data_array_t *pause_hotkey = nullptr;
		obs_data_array_t *unpause_hotkey = nullptr;
		obs_hotkey_pair_save(PauseHotkey, &pause_hotkey, &unpause_hotkey);
		obs_data_set_array(settings, "pause_hotkey", pause_hotkey);
		obs_data_set_array(settings, "unpause_hotkey", unpause_hotkey);
		obs_data_array_release(pause_hotkey);
		obs_data_array_release(unpause_hotkey);
	}
	if (extraHotkey != OBS_INVALID_HOTKEY_ID) {
		obs_data_array_t *extra_hotkey = obs_hotkey_save(extraHotkey);
		obs_data_set_array(settings, "extra_hotkey", extra_hotkey);
		obs_data_array_release(extra_hotkey);
	}
	if (splitHotkey != OBS_INVALID_HOTKEY_ID) {
		obs_data_array_t *split_hotkey = obs_hotkey_save(splitHotkey);
		obs_data_set_array(settings, "split_hotkey", split_hotkey);
		obs_data_array_release(split_hotkey);
	}
	if (chapterHotkey != OBS_INVALID_HOTKEY_ID) {
		obs_data_array_t *chapter_hotkey = obs_hotkey_save(chapterHotkey);
		obs_data_set_array(settings, "chapter_hotkey", chapter_hotkey);
		obs_data_array_release(chapter_hotkey);
	}
}

void OutputWidget::UpdateSettings(obs_data_t *data)
{
	obs_data_release(settings);
	settings = data;
	obs_data_addref(settings);
	UpdateCanvas();
}

void OutputWidget::UpdateCanvas()
{
	auto canvas_name = obs_data_get_string(settings, "canvas");
	auto video_encoders = obs_data_get_array(settings, "video_encoders");
	auto count = obs_data_array_count(video_encoders);
	QSet<QString> canvases;
	for (size_t i = 0; i < count; i++) {
		auto item = obs_data_array_item(video_encoders, i);
		if (!item) {
			continue;
		}
		auto cn = obs_data_get_string(item, "canvas");
		canvases.insert(QString::fromUtf8(cn[0] == '\0' ? obs_module_text("MainCanvas") : cn));
		obs_data_release(item);
	}
	canvasLabel->setText(canvases.isEmpty()
				     ? QString::fromUtf8(canvas_name[0] == '\0' ? obs_module_text("MainCanvas") : canvas_name)
				     : canvases.values().join(", "));
	obs_data_array_release(video_encoders);

	auto canvas = obs_data_get_array(current_profile_config, "canvas");
	count = obs_data_array_count(canvas);
	for (size_t i = 0; i < count; i++) {
		auto item = obs_data_array_item(canvas, i);
		if (!item) {
			continue;
		}
		auto cn = obs_data_get_string(item, "name");
		if (cn[0] != '\0' && strcmp(cn, canvas_name) == 0) {
			auto c = color_from_int(obs_data_get_int(item, "color"));
			setStyleSheet(QString(".output-frame { border: 2px solid %1;}").arg(c.name(QColor::HexRgb)));
			obs_data_release(item);
			break;
		}
		obs_data_release(item);
	}
	obs_data_array_release(canvas);
}

bool OutputWidget::AddChapter(const char *chapter_name)
{
	if (!runner->output()) {
		return false;
	}
	proc_handler_t *ph = obs_output_get_proc_handler(runner->output());
	calldata cd;
	calldata_init(&cd);
	calldata_set_string(&cd, "chapter_name", chapter_name);
	bool result = proc_handler_call(ph, "add_chapter", &cd);
	calldata_free(&cd);
	return result;
}

bool OutputWidget::StartOutput(std::function<void()> onStarted)
{
	if (runner->active()) {
		onStarted();
		return true;
	}
	runner->setOnStarted(onStarted);
	auto starting = StartOutput(true);
	if (!starting) {
		runner->setOnStarted(nullptr);
	}
	return starting;
}

void OutputWidget::StopOutput()
{
	runner->stop();
}

bool OutputWidget::IsStream() const
{
	const auto output_type = obs_data_get_string(settings, "type");
	if (strcmp(output_type, "ffmpeg") == 0) {
		return !obs_data_get_bool(settings, "save_file");
	}
	return (output_type[0] == '\0' || strcmp(output_type, "stream") == 0);
}

bool OutputWidget::IsRecord() const
{
	const auto output_type = obs_data_get_string(settings, "type");
	if (strcmp(output_type, "ffmpeg") == 0) {
		return obs_data_get_bool(settings, "save_file");
	}
	return (strcmp(output_type, "record") == 0 || strcmp(output_type, "backtrack") == 0);
}

const char *OutputWidget::GetOutputType() const
{
	const auto output_type = obs_data_get_string(settings, "type");
	if (output_type[0] == '\0') {
		return "stream";
	} else {
		return output_type;
	}
}

QString OutputWidget::GetCanvasName() const
{
	return QString::fromUtf8(obs_data_get_string(settings, "canvas"));
}

QString OutputWidget::GetGroup() const
{
	return QString::fromUtf8(obs_data_get_string(settings, "group")).trimmed();
}

QString OutputWidget::GetServer() const
{
	return QString::fromUtf8(obs_data_get_string(settings, "stream_server"));
}

bool OutputWidget::IsActive() const
{
	return runner->active();
}

// The plugin's events about this output (fiji_emit_event: obs-websocket, the deck pages, the core's state machines).
void OutputWidget::emitOutputEvent(const char *event, const char *lastError, long long code)
{
	if (!fiji_events_wanted()) {
		return;
	}
	OBSDataAutoRelease d = obs_data_create();
	obs_data_set_string(d, "output", objectName().toUtf8().constData());
	if (lastError && lastError[0]) {
		obs_data_set_string(d, "last_error", lastError);
	}
	if (strcmp(event, "stop_output") == 0) {
		obs_data_set_int(d, "code", code);
	}
	const QString canvas = runner->virtualCamCanvas();
	if (strcmp(event, "start_output") == 0 && !canvas.isEmpty()) {
		obs_data_set_string(d, "canvas", canvas.toUtf8().constData());
	}
	fiji_emit_event(event, d);
}

extern OutputDock *output_dock;
void save_current_profile_config(bool save_docks);

// Right-click an output to put it in a group; groups start and stop together from the Outputs dock.
void OutputWidget::contextMenuEvent(QContextMenuEvent *event)
{
	QMenu menu(this);
	auto groupMenu = menu.addMenu(QString::fromUtf8(obs_module_text("OutputGroup")));
	const auto current = GetGroup();
	auto setGroup = [this](const QString &group) {
		obs_data_set_string(settings, "group", group.toUtf8().constData());
		if (output_dock) {
			output_dock->RebuildGroups();
		}
		save_current_profile_config(false);
	};
	auto a = groupMenu->addAction(QString::fromUtf8(obs_module_text("OutputGroupNone")), [setGroup] { setGroup(QString()); });
	a->setCheckable(true);
	a->setChecked(current.isEmpty());
	const auto groups = output_dock ? output_dock->GetGroups() : QStringList();
	for (const auto &group : groups) {
		a = groupMenu->addAction(group, [setGroup, group] { setGroup(group); });
		a->setCheckable(true);
		a->setChecked(group == current);
	}
	groupMenu->addSeparator();
	groupMenu->addAction(QString::fromUtf8(obs_module_text("OutputGroupNew")), [this, setGroup] {
		std::string name;
		if (NameDialog::AskForName(this, QString::fromUtf8(obs_module_text("OutputGroupName")), name) && !name.empty()) {
			setGroup(QString::fromUtf8(name.c_str()).trimmed());
		}
	});
	menu.exec(event->globalPos());
}
