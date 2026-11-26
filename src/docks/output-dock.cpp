#include "output-dock.hpp"

#include "../version.h"
#include <obs-frontend-api.h>
#include <obs-module.h>
#include <obs.hpp>
#include <QCheckBox>
#include <QDateTime>
#include <QGroupBox>
#include <QIcon>
#include <QLabel>
#include <QMainWindow>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QToolBar>
#include <QToolButton>
#include <src/utils/color.hpp>
#include <src/utils/icon.hpp>
#include <util/config-file.h>
#include <util/platform.h>

void open_config_dialog(int tab, const char *create_type);

OutputDock::OutputDock(QWidget *parent) : QFrame(parent)
{

	auto main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());

	auto t = new QWidget;

	QScrollArea *scrollArea = new QScrollArea;
	scrollArea->setWidget(t);
	scrollArea->setWidgetResizable(true);
	scrollArea->setLineWidth(0);
	scrollArea->setFrameShape(QFrame::NoFrame);
	//scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

	auto layout = new QVBoxLayout;
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(scrollArea);

	auto toolbar = new QToolBar();

	toolbar->setObjectName(QStringLiteral("outputsToolbar"));
	toolbar->setIconSize(QSize(16, 16));
	toolbar->setFloatable(false);

	auto a = toolbar->addAction(QIcon(QString::fromUtf8(":/settings/images/settings/general.svg")),
				    QString::fromUtf8(obs_frontend_get_locale_string("Settings")),
				    [this] { open_config_dialog(2, nullptr); });
	toolbar->widgetForAction(a)->setProperty("themeID", QVariant(QString::fromUtf8("propertiesIconSmall")));
	toolbar->widgetForAction(a)->setProperty("class", "icon-gear");

	auto addMenu = new QMenu;

	a = toolbar->addAction(QIcon(":/res/images/plus.svg"), QString::fromUtf8(obs_module_text("AddOutput")),
			       [addMenu] { addMenu->exec(QCursor::pos()); });
	toolbar->widgetForAction(a)->setProperty("themeID", QVariant(QString::fromUtf8("addIconSmall")));
	toolbar->widgetForAction(a)->setProperty("class", "icon-plus");

	a = toolbar->addAction(
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
		QIcon::fromTheme(QIcon::ThemeIcon::MediaPlaybackStart, QIcon(":/res/images/media/media_play.svg")),
#else
		QIcon(":/res/images/media/media_play.svg"),
#endif
		QString::fromUtf8(obs_module_text("StartAll")));
	connect(a, &QAction::triggered, [this] {
		QMenu startMenu;
		auto a2 = startMenu.addAction(
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
			QIcon::fromTheme(QIcon::ThemeIcon::MediaPlaybackStart, QIcon(":/res/images/media/media_play.svg")),
#else
			QIcon(":/res/images/media/media_play.svg"),
#endif
			QString::fromUtf8(obs_module_text("StartAllOutputs")), [this]() { StartAll(false, false); });
		a2->setProperty("themeID", QVariant(QString::fromUtf8("playIcon")));
		a2->setProperty("class", "icon-media-play");
		if (mainStreamEnabled || std::find_if(outputWidgets.begin(), outputWidgets.end(),
						      [](OutputWidget *w) { return w->IsStream(); }) != outputWidgets.end()) {
			startMenu.addAction(QIcon(QString::fromUtf8(":/fiji/media/stream.svg")),
					    QString::fromUtf8(obs_module_text("StartAllStreams")),
					    [this]() { StartAll(true, false); });
		}
		if (mainRecordEnabled || std::find_if(outputWidgets.begin(), outputWidgets.end(),
						      [](OutputWidget *w) { return w->IsRecord(); }) != outputWidgets.end()) {
			startMenu.addAction(QIcon(QString::fromUtf8(":/fiji/media/record.svg")),
					    QString::fromUtf8(obs_module_text("StartAllRecordings")),
					    [this]() { StartAll(false, true); });
		}
		startMenu.exec(QCursor::pos());
	});
	toolbar->widgetForAction(a)->setProperty("themeID", QVariant(QString::fromUtf8("playIcon")));
	toolbar->widgetForAction(a)->setProperty("class", "icon-media-play");
	a->setIconText(QString::fromUtf8(obs_module_text("StartAll")));
	((QToolButton *)toolbar->widgetForAction(a))->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	toolbar->widgetForAction(a)->setStyleSheet("QToolButton { min-width: 80px; max-width: none; }");

	a = toolbar->addAction(
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
		QIcon::fromTheme(QIcon::ThemeIcon::MediaPlaybackStop, QIcon(":/res/images/media/media_stop.svg")),
#else
		QIcon(":/res/images/media/media_stop.svg"),
#endif
		QString::fromUtf8(obs_module_text("StopAllOutputs")), [this] { StopAll(false, false); });
	toolbar->widgetForAction(a)->setProperty("themeID", QVariant(QString::fromUtf8("stopIcon")));
	toolbar->widgetForAction(a)->setProperty("class", "icon-media-stop");

	a = addMenu->addAction(QIcon(QString::fromUtf8(":/fiji/media/stream.svg")), QString::fromUtf8(obs_module_text("Stream")),
			       [this] { open_config_dialog(2, "stream"); });

	a = addMenu->addAction(QIcon(QString::fromUtf8(":/fiji/media/record.svg")), QString::fromUtf8(obs_module_text("Record")),
			       [this] { open_config_dialog(2, "record"); });

	a = addMenu->addAction(QIcon(QString::fromUtf8(":/fiji/media/backtrack_off.svg")),
			       QString::fromUtf8(obs_module_text("Backtrack")), [this] { open_config_dialog(2, "backtrack"); });

	a = addMenu->addAction(QIcon(QString::fromUtf8(":/fiji/media/virtual_cam_off.svg")),
			       QString::fromUtf8(obs_module_text("VirtualCamera")),
			       [this] { open_config_dialog(2, "virtual_cam"); });

	a = addMenu->addAction(QIcon(QString::fromUtf8(":/fiji/media/ffmpeg_off.svg")),
			       QString::fromUtf8(obs_module_text("FfmpegOutput")), [this] { open_config_dialog(2, "ffmpeg"); });

	groupsLayout = new QVBoxLayout;
	groupsLayout->setContentsMargins(0, 0, 0, 0);
	layout->addLayout(groupsLayout);
	layout->addWidget(toolbar);

	setLayout(layout);

	mainLayout = new QVBoxLayout;
	mainLayout->setContentsMargins(0, 0, 0, 0);
	t->setLayout(mainLayout);

	mainPlatformIconLabel = new QLabel;
	mainPlatformIconLabel->setPixmap(
		getPlatformIconFromEndpoint(QString::fromUtf8("")).pixmap(outputPlatformIconSize, outputPlatformIconSize));

	auto l2 = new QHBoxLayout;
	l2->setContentsMargins(5, 0, 5, 0);
	l2->setSpacing(0);

	l2->addWidget(mainPlatformIconLabel);
	l2->addWidget(new QLabel(QString::fromUtf8(obs_module_text("BuiltinStream"))), 1);
	mainStreamCanvasLabel = new QLabel(QString::fromUtf8(obs_module_text("MainCanvas")));
	l2->addWidget(mainStreamCanvasLabel, 1);
	mainStreamButton = new QPushButton;
	mainStreamButton->setObjectName(QStringLiteral("canvasStream"));
	mainStreamButton->setMinimumHeight(30);
	mainStreamButton->setIcon(create2StateIcon(":/fiji/media/streaming.svg", ":/fiji/media/stream.svg"));
	mainStreamButton->setCheckable(true);
	mainStreamButton->setChecked(false);

	connect(mainStreamButton, &QPushButton::clicked, [this] {
		const auto config = obs_frontend_get_user_config();
		if (obs_frontend_streaming_active()) {
			bool stop = true;
			bool warnBeforeStreamStop = config_get_bool(config, "BasicWindow", "WarnBeforeStoppingStream");
			if (warnBeforeStreamStop && isVisible()) {
				auto button = QMessageBox::question(
					this, QString::fromUtf8(obs_frontend_get_locale_string("ConfirmStop.Title")),
					QString::fromUtf8(obs_frontend_get_locale_string("ConfirmStop.Text")),
					QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
				if (button == QMessageBox::No) {
					stop = false;
				}
			}
			if (stop) {
				obs_frontend_streaming_stop();
				mainStreamButton->setChecked(false);
			} else {
				mainStreamButton->setChecked(true);
			}
		} else {
			bool warnBeforeStreamStart = config_get_bool(config, "BasicWindow", "WarnBeforeStartingStream");
			if (warnBeforeStreamStart && isVisible()) {
				auto button = QMessageBox::question(
					this, QString::fromUtf8(obs_frontend_get_locale_string("ConfirmStart.Title")),
					QString::fromUtf8(obs_frontend_get_locale_string("ConfirmStart.Text")),
					QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
				if (button == QMessageBox::No) {
					mainStreamButton->setChecked(false);
				} else {
					obs_frontend_streaming_start();
				}
			} else {
				obs_frontend_streaming_start();
			}
			mainStreamButton->setChecked(true);
		}
	});
	//streamButton->setSizePolicy(sp2);
	mainStreamButton->setToolTip(QString::fromUtf8(obs_module_text("Stream")));
	l2->addWidget(mainStreamButton);
	//mainLayout->addLayout(l2);

	mainStreamGroup = new QFrame;
	mainStreamGroup->setLayout(l2);

	mainLayout->addWidget(mainStreamGroup);

	l2 = new QHBoxLayout;
	l2->setContentsMargins(5, 0, 5, 0);
	l2->setSpacing(0);
	l2->addWidget(new QLabel(QString::fromUtf8(obs_module_text("BuiltinRecord"))), 1);
	l2->addWidget(new QLabel(QString::fromUtf8(obs_module_text("MainCanvas"))), 1);
	mainRecordButton = new QPushButton;
	mainRecordButton->setObjectName(QStringLiteral("canvasRecord"));
	mainRecordButton->setMinimumHeight(30);
	mainRecordButton->setIcon(create2StateIcon(":/fiji/media/recording.svg", ":/fiji/media/record.svg"));
	mainRecordButton->setCheckable(true);
	mainRecordButton->setChecked(false);
	QObject::connect(mainRecordButton, SIGNAL(clicked()), main_window, SLOT(RecordActionTriggered()));

	l2->addWidget(mainRecordButton);

	mainRecordGroup = new QFrame;
	mainRecordGroup->setLayout(l2);
	mainLayout->addWidget(mainRecordGroup);

	l2 = new QHBoxLayout;
	l2->setContentsMargins(5, 0, 5, 0);
	l2->setSpacing(0);
	l2->addWidget(new QLabel(QString::fromUtf8(obs_module_text("BuiltinBacktrack"))), 1);
	l2->addWidget(new QLabel(QString::fromUtf8(obs_module_text("MainCanvas"))), 1);

	//mainBacktrackCheckbox =

	mainBacktrackCheckboxButton = new QPushButton;
	mainBacktrackCheckboxButton->setCheckable(true);
	mainBacktrackCheckboxButton->setStyleSheet(QString::fromUtf8(
		"QPushButton{ border-top-right-radius: 0; border-bottom-right-radius: 0; width: 32px; padding-left: 0px; padding-right: 0px;}"));

	auto buttonLayout = new QHBoxLayout;
	mainBacktrackCheckboxButton->setLayout(buttonLayout);

	mainBacktrackCheckbox = new QCheckBox;
	buttonLayout->addWidget(mainBacktrackCheckbox);

	mainBacktrackButton = new QPushButton;
	mainBacktrackButton->setObjectName(QStringLiteral("canvasBacktrack"));
	mainBacktrackButton->setMinimumHeight(30);
	mainBacktrackButton->setIcon(create2StateIcon(":/fiji/media/backtrack_on.svg", ":/fiji/media/backtrack_off.svg"));
	mainBacktrackButton->setStyleSheet(
		"QPushButton{min-width: 32px; padding-left: 0px; padding-right: 0px; border-top-left-radius: 0; border-bottom-left-radius: 0;}");
	mainBacktrackButton->setCheckable(true);
	mainBacktrackButton->setChecked(false);
	QObject::connect(mainBacktrackButton, &QPushButton::clicked, [this] {
		if (obs_frontend_replay_buffer_active()) {
			obs_frontend_replay_buffer_save();
			mainBacktrackButton->setChecked(true);
			mainBacktrackStartTime = QDateTime::currentDateTime();
		} else {
			mainBacktrackButton->setChecked(false);
		}
	});

	connect(mainBacktrackCheckboxButton, &QPushButton::clicked, [this] {
		bool enabled = mainBacktrackCheckboxButton->isChecked();
		if (enabled != mainBacktrackCheckbox->isChecked()) {
			mainBacktrackCheckbox->setChecked(enabled);
		}
		if (enabled != mainBacktrackButton->isChecked()) {
			mainBacktrackButton->setChecked(enabled);
		}
		if (enabled) {
			obs_frontend_replay_buffer_start();
		} else {
			obs_frontend_replay_buffer_stop();
		}
	});
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
	connect(mainBacktrackCheckbox, &QCheckBox::checkStateChanged, [this] {
#else
	connect(mainBacktrackCheckbox, &QCheckBox::stateChanged, [this] {
#endif
		if (mainBacktrackCheckbox->isChecked() != mainBacktrackCheckboxButton->isChecked()) {
			mainBacktrackCheckboxButton->click();
		}
	});

	auto l3 = new QHBoxLayout;
	l3->setContentsMargins(0, 0, 0, 0);
	l3->setSpacing(0);
	l3->addWidget(mainBacktrackCheckboxButton);
	l3->addWidget(mainBacktrackButton);
	l2->addLayout(l3);

	mainBacktrackGroup = new QFrame;
	mainBacktrackGroup->setLayout(l2);
	mainLayout->addWidget(mainBacktrackGroup);

	l2 = new QHBoxLayout;
	l2->setContentsMargins(5, 0, 5, 0);
	l2->setSpacing(0);
	l2->addWidget(new QLabel(QString::fromUtf8(obs_module_text("BuiltinVirtualCamera"))), 1);
	l2->addWidget(new QLabel(QString::fromUtf8(obs_module_text("MainCanvas"))), 1);
	mainVirtualCamButton = new QPushButton;
	mainVirtualCamButton->setObjectName(QStringLiteral("canvasVirtualCamera"));
	mainVirtualCamButton->setMinimumHeight(30);
	mainVirtualCamButton->setIcon(create2StateIcon(":/fiji/media/virtual_cam_on.svg", ":/fiji/media/virtual_cam_off.svg"));
	mainVirtualCamButton->setCheckable(true);
	mainVirtualCamButton->setChecked(false);

	connect(mainVirtualCamButton, &QPushButton::clicked, [this] {
		if (obs_frontend_virtualcam_active()) {
			obs_frontend_stop_virtualcam();
			mainVirtualCamButton->setChecked(false);
		} else {
			obs_frontend_start_virtualcam();
			mainVirtualCamButton->setChecked(true);
		}
	});

	l2->addWidget(mainVirtualCamButton);

	mainVirtualCamGroup = new QFrame;
	mainVirtualCamGroup->setLayout(l2);
	mainLayout->addWidget(mainVirtualCamGroup);

	connect(&videoCheckTimer, &QTimer::timeout, [this] {
		if (exiting) {
			return;
		}

		if (mainPlatformIconLabel) {
			auto service = obs_frontend_get_streaming_service();
			auto url = QString::fromUtf8(
				service ? obs_service_get_connect_info(service, OBS_SERVICE_CONNECT_INFO_SERVER_URL) : "");
			if (url != mainPlatformUrl) {
				mainPlatformUrl = url;
				mainPlatformIconLabel->setPixmap(
					getPlatformIconFromEndpoint(url).pixmap(outputPlatformIconSize, outputPlatformIconSize));
			}
		}

		if (mainStreamEnabled) {
			auto active = obs_frontend_streaming_active();
			if (mainStreamButton->isChecked() != active) {
				mainStreamButton->setChecked(active);
				if (!active) {
					mainStreamButton->setText("");
				}
			} else if (active) {
				auto t = QTime::fromMSecsSinceStartOfDay(mainStreamStartTime.msecsTo(QDateTime::currentDateTime()));
				mainStreamButton->setText(t.toString(t.hour() ? "hh:mm:ss" : "mm:ss"));
			}
		}

		if (mainRecordEnabled) {
			auto active = obs_frontend_recording_active();
			if (mainRecordButton->isChecked() != active) {
				mainRecordButton->setChecked(active);
				if (!active) {
					mainRecordButton->setText("");
				}
			} else if (active) {
				auto t = QTime::fromMSecsSinceStartOfDay(mainRecordStartTime.msecsTo(QDateTime::currentDateTime()));
				mainRecordButton->setText(t.toString(t.hour() ? "hh:mm:ss" : "mm:ss"));
			}
		}

		if (mainBacktrackEnabled) {
			auto enabled = obs_frontend_replay_buffer_active();
			if (mainBacktrackCheckboxButton->isChecked() != enabled) {
				mainBacktrackCheckboxButton->setChecked(enabled);
				if (!enabled) {
					mainBacktrackButton->setText("");
				}
			} else if (enabled) {
				auto t = QTime::fromMSecsSinceStartOfDay(
					mainBacktrackStartTime.msecsTo(QDateTime::currentDateTime()));
				mainBacktrackButton->setText(t.toString(t.hour() ? "hh:mm:ss" : "mm:ss"));
			}
			if (mainBacktrackCheckbox->isChecked() != enabled) {
				mainBacktrackCheckbox->setChecked(enabled);
			}
			if (mainBacktrackButton->isChecked() != enabled) {
				mainBacktrackButton->setChecked(enabled);
			}
		}

		if (mainVirtualCamEnabled) {
			auto active = obs_frontend_virtualcam_active();
			if (mainVirtualCamButton->isChecked() != active) {
				mainVirtualCamButton->setChecked(active);
				if (!active) {
					mainVirtualCamButton->setText("");
				}
			} else if (active) {
				auto t = QTime::fromMSecsSinceStartOfDay(
					mainVirtualCamStartTime.msecsTo(QDateTime::currentDateTime()));
				mainVirtualCamButton->setText(t.toString(t.hour() ? "hh:mm:ss" : "mm:ss"));
			}
		}

		for (auto it = outputWidgets.begin(); it != outputWidgets.end(); it++) {
			(*it)->CheckActive();
		}
		UpdateGroupStatus();
	});
	videoCheckTimer.start(500);
	obs_frontend_add_event_callback(frontend_event, this);

	StartStopHotkey = obs_hotkey_pair_register_frontend(
		"FijiSuiteStartAllOutputs", obs_module_text("StartAllOutputs"), "FijiSuiteStopAllOutputs",
		obs_module_text("StopAllOutputs"),
		[](void *data, obs_hotkey_pair_id id, obs_hotkey_t *hotkey, bool pressed) {
			UNUSED_PARAMETER(id);
			UNUSED_PARAMETER(hotkey);
			if (!pressed) {
				return false;
			}
			auto dock = static_cast<OutputDock *>(data);
			QMetaObject::invokeMethod(dock, [dock] { dock->StartAll(false, false); });
			return true;
		},
		[](void *data, obs_hotkey_pair_id id, obs_hotkey_t *hotkey, bool pressed) {
			UNUSED_PARAMETER(id);
			UNUSED_PARAMETER(hotkey);
			if (!pressed) {
				return false;
			}
			auto dock = static_cast<OutputDock *>(data);
			QMetaObject::invokeMethod(dock, [dock] { dock->StopAll(false, false); });
			return true;
		},
		this, this);

	StartStreamHotkey = obs_hotkey_register_frontend(
		"FijiSuiteStartAllStreams", obs_module_text("StartAllStreams"),
		[](void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed) {
			UNUSED_PARAMETER(id);
			UNUSED_PARAMETER(hotkey);
			if (!pressed) {
				return;
			}
			auto dock = static_cast<OutputDock *>(data);
			QMetaObject::invokeMethod(dock, [dock] { dock->StartAll(true, false); });
		},
		this);

	StartRecordHotkey = obs_hotkey_register_frontend(
		"FijiSuiteStartAllRecordings", obs_module_text("StartAllRecordings"),
		[](void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed) {
			UNUSED_PARAMETER(id);
			UNUSED_PARAMETER(hotkey);
			if (!pressed) {
				return;
			}
			auto dock = static_cast<OutputDock *>(data);
			QMetaObject::invokeMethod(dock, [dock] { dock->StartAll(false, true); });
		},
		this);
}

extern OutputDock *output_dock;
void RemoveWidget(QWidget *widget);

OutputDock::~OutputDock()
{
	if (StartStopHotkey != OBS_INVALID_HOTKEY_PAIR_ID) {
		obs_hotkey_pair_unregister(StartStopHotkey);
	}
	if (StartStreamHotkey != OBS_INVALID_HOTKEY_ID) {
		obs_hotkey_unregister(StartStreamHotkey);
	}
	if (StartRecordHotkey != OBS_INVALID_HOTKEY_ID) {
		obs_hotkey_unregister(StartRecordHotkey);
	}
	obs_frontend_remove_event_callback(frontend_event, this);
	videoCheckTimer.stop();
	ClearGroupRows();
	for (auto it = outputWidgets.begin(); it != outputWidgets.end(); it++) {
		mainLayout->removeWidget(*it);
		RemoveWidget(*it);
	}
	outputWidgets.clear();
	output_dock = nullptr;
}

extern obs_data_t *current_profile_config;

void OutputDock::LoadSettings()
{
	mainStreamEnabled = obs_data_get_bool(current_profile_config, "main_stream_output_show");
	mainStreamGroup->setVisible(mainStreamEnabled);

	mainRecordEnabled = obs_data_get_bool(current_profile_config, "main_record_output_show");
	mainRecordGroup->setVisible(mainRecordEnabled);

	mainBacktrackEnabled = obs_data_get_bool(current_profile_config, "main_backtrack_output_show");
	mainBacktrackGroup->setVisible(mainBacktrackEnabled);

	mainVirtualCamEnabled = obs_data_get_bool(current_profile_config, "main_virtual_cam_output_show");
	mainVirtualCamGroup->setVisible(mainVirtualCamEnabled);

	auto outputs2 = obs_data_get_array(current_profile_config, "outputs");
	for (auto it = outputWidgets.begin(); it != outputWidgets.end();) {
		bool found = false;
		auto count = obs_data_array_count(outputs2);
		for (size_t i = 0; i < count; i++) {
			obs_data_t *data2 = obs_data_array_item(outputs2, i);
			auto name = QString::fromUtf8(obs_data_get_string(data2, "name"));
			obs_data_release(data2);
			if (name == (*it)->objectName()) {
				(*it)->UpdateSettings(data2);
				auto j = mainLayout->indexOf(*it);
				if (j >= 0 && i + 4 != (size_t)j) {
					mainLayout->takeAt(j);
					mainLayout->insertWidget((int)i + 4, (*it));
				}
				found = true;
				break;
			}
		}
		if (!found) {
			mainLayout->removeWidget(*it);
			RemoveWidget(*it);
			it = outputWidgets.erase(it);
		} else {
			it++;
		}
	}

	auto count = obs_data_array_count(outputs2);
	for (size_t i = 0; i < count; i++) {
		obs_data_t *data2 = obs_data_array_item(outputs2, i);
		auto name = QString::fromUtf8(obs_data_get_string(data2, "name"));
		if (std::find_if(outputWidgets.begin(), outputWidgets.end(),
				 [&name](OutputWidget *ow) { return ow->objectName() == name; }) == outputWidgets.end()) {
			auto outputWidget = new OutputWidget(data2, this);
			outputWidgets.push_back(outputWidget);
			mainLayout->insertWidget((int)i + 4, outputWidget);
		}
		obs_data_release(data2);
	}

	obs_data_array_release(outputs2);

	QWidget *spacer = new QWidget();
	spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	mainLayout->addWidget(spacer);

	auto start_hotkey = obs_data_get_array(current_profile_config, "start_all_outputs_hotkey");
	auto stop_hotkey = obs_data_get_array(current_profile_config, "stop_all_outputs_hotkey");
	obs_hotkey_pair_load(StartStopHotkey, start_hotkey, stop_hotkey);
	obs_data_array_release(start_hotkey);
	obs_data_array_release(stop_hotkey);

	auto stream_hotkey = obs_data_get_array(current_profile_config, "start_all_streams_hotkey");
	obs_hotkey_load(StartStreamHotkey, stream_hotkey);
	obs_data_array_release(stream_hotkey);

	auto record_hotkey = obs_data_get_array(current_profile_config, "start_all_recordings_hotkey");
	obs_hotkey_load(StartRecordHotkey, record_hotkey);
	obs_data_array_release(record_hotkey);

	RebuildGroups();
}

void OutputDock::SaveSettings()
{
	SaveGroupHotkeys();
	for (auto it = outputWidgets.begin(); it != outputWidgets.end(); it++) {
		(*it)->SaveSettings();
	}

	if (StartStopHotkey != OBS_INVALID_HOTKEY_PAIR_ID) {
		obs_data_array_t *start_hotkey = nullptr;
		obs_data_array_t *stop_hotkey = nullptr;
		obs_hotkey_pair_save(StartStopHotkey, &start_hotkey, &stop_hotkey);
		obs_data_set_array(current_profile_config, "start_all_outputs_hotkey", start_hotkey);
		obs_data_set_array(current_profile_config, "stop_all_outputs_hotkey", stop_hotkey);
		obs_data_array_release(start_hotkey);
		obs_data_array_release(stop_hotkey);
	}
	if (StartStreamHotkey != OBS_INVALID_HOTKEY_ID) {
		obs_data_array_t *stream_hotkey = obs_hotkey_save(StartStreamHotkey);
		obs_data_set_array(current_profile_config, "start_all_streams_hotkey", stream_hotkey);
		obs_data_array_release(stream_hotkey);
	}
	if (StartRecordHotkey != OBS_INVALID_HOTKEY_ID) {
		obs_data_array_t *record_hotkey = obs_hotkey_save(StartRecordHotkey);
		obs_data_set_array(current_profile_config, "start_all_recordings_hotkey", record_hotkey);
		obs_data_array_release(record_hotkey);
	}
}

void OutputDock::UpdateMainStreamStatus(bool active)
{
	mainStreamButton->setChecked(active);
	if (active) {
		mainStreamStartTime = QDateTime::currentDateTime();
		mainStreamButton->setText("00:00");
	} else {
		mainStreamButton->setText("");
		return;
	}
	if (!current_profile_config) {
		return;
	}

	std::map<video_t *, std::string> canvasNames;
	obs_enum_canvases(
		[](void *data, obs_canvas_t *c) {
			auto v = obs_canvas_get_video(c);
			if (!v) {
				return true;
			}
			auto cns = static_cast<std::map<video_t *, std::string> *>(data);
			(*cns)[obs_canvas_get_video(c)] = obs_canvas_get_name(c);
			return true;
		},
		&canvasNames);

	struct obs_video_info ovi = {0};
	obs_get_video_info(&ovi);
	double fps = ovi.fps_den > 0 ? (double)ovi.fps_num / (double)ovi.fps_den : 0.0;
	auto output = obs_frontend_get_streaming_output();
	QStringList canvasNamesList;
	bool found = false;
	for (auto i = 0; i < MAX_OUTPUT_VIDEO_ENCODERS; i++) {
		auto encoder = obs_output_get_video_encoder2(output, i);
		QString settingName = QString::fromUtf8("video_encoder_description") + QString::number(i);
		if (encoder) {
			found = true;

			QString mainEncoderDescription;
			auto v = obs_encoder_parent_video(encoder);
			auto f = canvasNames.find(v);
			if (f != canvasNames.end()) {
				mainEncoderDescription = QString::fromUtf8(f->second.c_str());
				if (!canvasNamesList.contains(mainEncoderDescription)) {
					canvasNamesList.push_back(mainEncoderDescription);
				}
			}
			if (!mainEncoderDescription.isEmpty()) {
				mainEncoderDescription += QString::fromUtf8(" ");
			}
			mainEncoderDescription += QString::number(obs_encoder_get_width(encoder)) + "x" + QString::number(obs_encoder_get_height(encoder));
			auto divisor = obs_encoder_get_frame_rate_divisor(encoder);
			if (divisor > 0) {
				mainEncoderDescription +=
					QString::fromUtf8(" ") + QString::number(fps / divisor, 'g', 4) + QString::fromUtf8("fps");
			}

			auto settings = obs_encoder_get_settings(encoder);
			auto bitrate = settings ? obs_data_get_int(settings, "bitrate") : 0;
			if (bitrate > 0) {
				mainEncoderDescription +=
					QString::fromUtf8(" ") + QString::number(bitrate) + QString::fromUtf8("Kbps");
			}
			obs_data_release(settings);

			obs_data_set_string(current_profile_config, settingName.toUtf8().constData(),
					    mainEncoderDescription.toUtf8().constData());

		} else if (found && obs_data_has_user_value(current_profile_config, settingName.toUtf8().constData())) {
			obs_data_unset_user_value(current_profile_config, settingName.toUtf8().constData());
		}
	}
	obs_output_release(output);
	if (canvasNamesList.count() > 1) {
		mainStreamCanvasLabel->setText(canvasNamesList.join(", "));
	} else {
		mainStreamCanvasLabel->setText(QString::fromUtf8(obs_module_text("MainCanvas")));
	}
}

void OutputDock::UpdateMainRecordingStatus(bool active)
{
	mainRecordButton->setChecked(active);
	if (active) {
		mainRecordStartTime = QDateTime::currentDateTime();
		mainRecordButton->setText("00:00");
	} else {
		mainRecordButton->setText("");
	}
}

void OutputDock::UpdateMainBacktrackStatus(bool active)
{
	mainBacktrackCheckboxButton->setChecked(active);
	mainBacktrackCheckbox->setChecked(active);
	mainBacktrackButton->setChecked(active);
	if (active) {
		mainBacktrackStartTime = QDateTime::currentDateTime();
		mainBacktrackButton->setText("00:00");
	} else {
		mainBacktrackButton->setText("");
	}
}

void OutputDock::UpdateMainVirtualCameraStatus(bool active)
{
	mainVirtualCamButton->setChecked(active);
	if (active) {
		mainVirtualCamStartTime = QDateTime::currentDateTime();
		mainVirtualCamButton->setText("00:00");
	} else {
		mainVirtualCamButton->setText("");
	}
}

obs_data_array_t *OutputDock::GetOutputsArray()
{
	auto outputs2 = obs_data_array_create();
	for (auto it = outputWidgets.begin(); it != outputWidgets.end(); it++) {
		auto data = obs_data_create();
		obs_data_set_string(data, "name", (*it)->objectName().toUtf8().constData());
		obs_data_set_string(data, "type", (*it)->GetOutputType());
		obs_data_set_string(data, "canvas", (*it)->GetCanvasName().toUtf8().constData());
		obs_data_set_string(data, "group", (*it)->GetGroup().toUtf8().constData());
		auto o = (*it)->GetOutput();
		obs_data_set_bool(data, "active", o ? obs_output_active(o) : false);
		obs_data_set_bool(data, "reconnecting", o ? obs_output_reconnecting(o) : false);
		obs_data_set_int(data, "total_frames", o ? obs_output_get_total_frames(o) : 0);
		obs_data_set_int(data, "dropped_frames", o ? obs_output_get_frames_dropped(o) : 0);
		obs_data_set_int(data, "total_bytes", o ? obs_output_get_total_bytes(o) : 0);
		obs_data_array_push_back(outputs2, data);
		obs_data_release(data);
	}
	return outputs2;
}

bool OutputDock::AddChapterToOutput(const char *output_name, const char *chapter_name)
{
	auto on = QString::fromUtf8(output_name);
	for (auto it = outputWidgets.begin(); it != outputWidgets.end(); it++) {
		if ((*it)->objectName() == on) {
			return (*it)->AddChapter(chapter_name);
		}
	}
	return false;
}

void OutputDock::frontend_event(enum obs_frontend_event event, void *private_data)
{
	auto dock = static_cast<OutputDock *>(private_data);
	if (!dock) {
		return;
	}
	switch (event) {
	case OBS_FRONTEND_EVENT_FINISHED_LOADING:
		dock->frontendReady = true;
		break;
	case OBS_FRONTEND_EVENT_EXIT:
		dock->frontendReady = false;
		break;
	case OBS_FRONTEND_EVENT_STREAMING_STARTED:
		if (dock->mainStreamOnStarted) {
			blog(LOG_INFO, "[Fiji Suite] Main stream started, starting next output");
			dock->mainStreamOnStarted();
			dock->mainStreamOnStarted = nullptr;
		}
		break;
	case OBS_FRONTEND_EVENT_STREAMING_STOPPED:
		if (dock->mainStreamOnStarted) {
			blog(LOG_INFO, "[Fiji Suite] Main stream stopped, starting next output");
			dock->mainStreamOnStarted();
			dock->mainStreamOnStarted = nullptr;
		}
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STARTED:
		if (dock->mainRecordOnStarted) {
			blog(LOG_INFO, "[Fiji Suite] Main recording started, starting next output");
			dock->mainRecordOnStarted();
			dock->mainRecordOnStarted = nullptr;
		}
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STOPPED:
		if (dock->mainRecordOnStarted) {
			blog(LOG_INFO, "[Fiji Suite] Main recording stopped, starting next output");
			dock->mainRecordOnStarted();
			dock->mainRecordOnStarted = nullptr;
		}
		break;
	case OBS_FRONTEND_EVENT_REPLAY_BUFFER_STARTED:
		if (dock->mainBacktrackOnStarted) {
			blog(LOG_INFO, "[Fiji Suite] Main replay buffer started, starting next output");
			dock->mainBacktrackOnStarted();
			dock->mainBacktrackOnStarted = nullptr;
		}
		break;
	case OBS_FRONTEND_EVENT_REPLAY_BUFFER_STOPPED:
		if (dock->mainBacktrackOnStarted) {
			blog(LOG_INFO, "[Fiji Suite] Main replay buffer stopped, starting next output");
			dock->mainBacktrackOnStarted();
			dock->mainBacktrackOnStarted = nullptr;
		}
		break;
	case OBS_FRONTEND_EVENT_VIRTUALCAM_STARTED:
		if (dock->mainVirtualCamOnStarted) {
			blog(LOG_INFO, "[Fiji Suite] Main virtual camera started, starting next output");
			dock->mainVirtualCamOnStarted();
			dock->mainVirtualCamOnStarted = nullptr;
		}
		break;
	case OBS_FRONTEND_EVENT_VIRTUALCAM_STOPPED:
		if (dock->mainVirtualCamOnStarted) {
			blog(LOG_INFO, "[Fiji Suite] Main virtual camera stopped, starting next output");
			dock->mainVirtualCamOnStarted();
			dock->mainVirtualCamOnStarted = nullptr;
		}
		break;
	default:
		break;
	}
}

void OutputDock::StartAll(bool streamOnly, bool recordOnly)
{
	QStringList ids;
	if (mainStreamEnabled && !recordOnly) {
		bool warnBeforeStreamStart =
			config_get_bool(obs_frontend_get_user_config(), "BasicWindow", "WarnBeforeStartingStream");
		if (warnBeforeStreamStart && isVisible()) {
			auto button = QMessageBox::question(this,
							    QString::fromUtf8(obs_frontend_get_locale_string("ConfirmStart.Title")),
							    QString::fromUtf8(obs_frontend_get_locale_string("ConfirmStart.Text")),
							    QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
			if (button == QMessageBox::No) {
				return;
			}
		}
		ids.append(QStringLiteral("main:stream"));
	}
	if (mainRecordEnabled && !streamOnly) {
		ids.append(QStringLiteral("main:record"));
	}
	if (mainBacktrackEnabled && !streamOnly) {
		ids.append(QStringLiteral("main:backtrack"));
	}
	if (mainVirtualCamEnabled && !streamOnly && !recordOnly) {
		ids.append(QStringLiteral("main:virtual_cam"));
	}
	for (auto &ow : outputWidgets) {
		if ((streamOnly && !ow->IsStream()) || (recordOnly && !ow->IsRecord())) {
			continue;
		}
		ids.append(QStringLiteral("output:") + ow->objectName());
	}
	if (ids.isEmpty()) {
		return;
	}
	blog(LOG_INFO, "[Fiji Suite] Starting %d outputs", int(ids.size()));
	sequencer.start(ids);
}

OutputWidget *OutputDock::widgetNamed(const QString &name) const
{
	for (auto ow : outputWidgets) {
		if (ow->objectName() == name) {
			return ow;
		}
	}
	return nullptr;
}

// One output, for the sequencer. OBS's own outputs report "live" through the frontend events (frontend_event).
bool OutputDock::startOutput(const QString &id, std::function<void()> onLive)
{
	if (id.startsWith(QStringLiteral("output:"))) {
		auto ow = widgetNamed(id.mid(7));
		return ow && ow->StartOutput(onLive);
	}
	const QString kind = id.mid(5);
	if (kind == QStringLiteral("stream")) {
		if (obs_frontend_streaming_active()) {
			blog(LOG_INFO, "[Fiji Suite] Skipped starting main stream, already active");
			onLive();
			return true;
		}
		mainStreamOnStarted = onLive;
		obs_frontend_streaming_start();
		return true;
	}
	// recording, replay buffer and virtual camera may have no output at all (not set up): then nothing waits
	const auto start_main = [&](bool active, const char *what, std::function<void()> &slot, void (*start)(),
				    obs_output_t *(*output)()) {
		if (active) {
			blog(LOG_INFO, "[Fiji Suite] Skipped starting main %s, already active", what);
			onLive();
			return true;
		}
		slot = onLive;
		start();
		OBSOutputAutoRelease o = output();
		if (!o) {
			blog(LOG_INFO, "[Fiji Suite] No main %s output found", what);
			slot = nullptr;
			onLive();
		}
		return true;
	};
	if (kind == QStringLiteral("record")) {
		return start_main(obs_frontend_recording_active(), "recording", mainRecordOnStarted, obs_frontend_recording_start,
				  obs_frontend_get_recording_output);
	}
	if (kind == QStringLiteral("backtrack")) {
		return start_main(obs_frontend_replay_buffer_active(), "replay buffer", mainBacktrackOnStarted,
				  obs_frontend_replay_buffer_start, obs_frontend_get_replay_buffer_output);
	}
	if (kind == QStringLiteral("virtual_cam")) {
		return start_main(obs_frontend_virtualcam_active(), "virtual camera", mainVirtualCamOnStarted,
				  obs_frontend_start_virtualcam, obs_frontend_get_virtualcam_output);
	}
	return false;
}

void OutputDock::StopAll(bool streamOnly, bool recordOnly)
{
	sequencer.stop();
	bool warnStream = config_get_bool(obs_frontend_get_user_config(), "BasicWindow", "WarnBeforeStoppingStream");
	bool warnRecord = config_get_bool(obs_frontend_get_user_config(), "BasicWindow", "WarnBeforeStoppingRecord");
	if (warnStream && mainStreamEnabled && !recordOnly && obs_frontend_streaming_active() && isVisible()) {
		auto button = QMessageBox::question(this, QString::fromUtf8(obs_frontend_get_locale_string("ConfirmStop.Title")),
						    QString::fromUtf8(obs_frontend_get_locale_string("ConfirmStop.Text")),
						    QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
		if (button == QMessageBox::No) {
			return;
		}
	} else if (warnRecord && mainRecordEnabled && !streamOnly && obs_frontend_recording_active() && isVisible()) {
		auto button = QMessageBox::question(this,
						    QString::fromUtf8(obs_frontend_get_locale_string("ConfirmStopRecord.Title")),
						    QString::fromUtf8(obs_frontend_get_locale_string("ConfirmStopRecord.Text")),
						    QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
		if (button == QMessageBox::No) {
			return;
		}
	}

	if (mainStreamEnabled && !recordOnly && obs_frontend_streaming_active()) {
		obs_frontend_streaming_stop();
	}
	if (mainRecordEnabled && !streamOnly && obs_frontend_recording_active()) {
		obs_frontend_recording_stop();
	}
	if (mainBacktrackEnabled && !streamOnly && obs_frontend_replay_buffer_active()) {
		obs_frontend_replay_buffer_stop();
	}
	if (mainVirtualCamEnabled && !streamOnly && !recordOnly && obs_frontend_virtualcam_active()) {
		obs_frontend_stop_virtualcam();
	}
	for (auto &ow : outputWidgets) {
		if (streamOnly && !ow->IsStream()) {
			continue;
		}
		if (recordOnly && !ow->IsRecord()) {
			continue;
		}
		ow->StopOutput();
	}
}

std::vector<OutputState> OutputDock::GetOutputStates() const
{
	std::vector<OutputState> states;
	auto fill = [](OutputState &s, obs_output_t *o) {
		s.active = o && obs_output_active(o);
		s.reconnecting = o && obs_output_reconnecting(o);
		s.totalBytes = o ? obs_output_get_total_bytes(o) : 0;
		s.totalFrames = o ? obs_output_get_total_frames(o) : 0;
		s.droppedFrames = o ? obs_output_get_frames_dropped(o) : 0;
	};
	auto addMain = [&](bool enabled, const char *type, const char *label, obs_output_t *o, const QDateTime &start) {
		OBSOutputAutoRelease output = o;
		if (!enabled) {
			return;
		}
		OutputState s;
		s.name = QString::fromUtf8(obs_module_text(label));
		s.type = type;
		s.main = true;
		s.startTime = start;
		fill(s, output);
		states.push_back(s);
	};
	if (frontendReady && !exiting) {
		addMain(mainStreamEnabled, "stream", "Stream", obs_frontend_get_streaming_output(), mainStreamStartTime);
		if (!states.empty()) {
			states.back().server = mainPlatformUrl;
		}
		addMain(mainRecordEnabled, "record", "Record", obs_frontend_get_recording_output(), mainRecordStartTime);
		addMain(mainBacktrackEnabled, "backtrack", "Backtrack", obs_frontend_get_replay_buffer_output(),
			mainBacktrackStartTime);
		addMain(mainVirtualCamEnabled, "virtual_cam", "VirtualCamera", obs_frontend_get_virtualcam_output(),
			mainVirtualCamStartTime);
	}

	for (const auto &ow : outputWidgets) {
		OutputState s;
		s.name = ow->objectName();
		s.type = QString::fromUtf8(ow->GetOutputType());
		s.canvas = ow->GetCanvasName();
		s.group = ow->GetGroup();
		s.server = ow->GetServer();
		s.startTime = ow->GetStartTime();
		fill(s, ow->GetOutput());
		states.push_back(s);
	}
	return states;
}

QStringList OutputDock::GetGroups() const
{
	QStringList groups;
	for (const auto &ow : outputWidgets) {
		groups.append(ow->GetGroup());
	}
	return fiji::model::groupsOf(groups);
}

void OutputDock::StartOutputs(const std::function<bool(OutputWidget *)> &filter)
{
	QStringList ids;
	for (auto &ow : outputWidgets) {
		if (filter(ow)) {
			ids.append(QStringLiteral("output:") + ow->objectName());
		}
	}
	if (ids.isEmpty()) {
		return;
	}
	blog(LOG_INFO, "[Fiji Suite] Starting %d outputs", int(ids.size()));
	sequencer.start(ids);
}

void OutputDock::StartGroup(const QString &group)
{
	StartOutputs([&group](OutputWidget *ow) { return ow->GetGroup() == group; });
}

void OutputDock::StopGroup(const QString &group)
{
	sequencer.stop();
	for (auto &ow : outputWidgets) {
		if (ow->GetGroup() == group) {
			ow->StopOutput();
		}
	}
}

void OutputDock::SaveGroupHotkeys()
{
	if (!current_profile_config || groupRows.empty()) {
		return;
	}
	OBSDataAutoRelease hotkeys = obs_data_get_obj(current_profile_config, "group_hotkeys");
	if (!hotkeys) {
		hotkeys = obs_data_create();
		obs_data_set_obj(current_profile_config, "group_hotkeys", hotkeys);
	}
	for (auto &g : groupRows) {
		if (g.hotkey == OBS_INVALID_HOTKEY_PAIR_ID) {
			continue;
		}
		obs_data_array_t *start = nullptr;
		obs_data_array_t *stop = nullptr;
		obs_hotkey_pair_save(g.hotkey, &start, &stop);
		OBSDataAutoRelease h = obs_data_create();
		obs_data_set_array(h, "start", start);
		obs_data_set_array(h, "stop", stop);
		obs_data_set_obj(hotkeys, g.name.toUtf8().constData(), h);
		obs_data_array_release(start);
		obs_data_array_release(stop);
	}
}

void OutputDock::ClearGroupRows()
{
	for (auto &g : groupRows) {
		if (g.hotkey != OBS_INVALID_HOTKEY_PAIR_ID) {
			obs_hotkey_pair_unregister(g.hotkey);
		}
		groupsLayout->removeWidget(g.row);
		delete g.row;
	}
	groupRows.clear();
}

// One row per group under the output list: a live count and Start / Stop for the whole group.
void OutputDock::RebuildGroups()
{
	SaveGroupHotkeys();
	ClearGroupRows();
	OBSDataAutoRelease hotkeys = current_profile_config ? obs_data_get_obj(current_profile_config, "group_hotkeys") : nullptr;
	const auto groups = GetGroups();
	for (const auto &name : groups) {
		groupRows.emplace_back();
		auto &g = groupRows.back();
		g.name = name;
		g.row = new QFrame;
		auto rowLayout = new QHBoxLayout;
		rowLayout->setContentsMargins(5, 2, 5, 2);
		g.label = new QLabel(name);
		rowLayout->addWidget(g.label, 1);
		auto start = new QPushButton(QString::fromUtf8(obs_module_text("StartHotkey")));
		connect(start, &QPushButton::clicked, this, [this, name] { StartGroup(name); });
		rowLayout->addWidget(start);
		auto stop = new QPushButton(QString::fromUtf8(obs_module_text("StopHotkey")));
		connect(stop, &QPushButton::clicked, this, [this, name] { StopGroup(name); });
		rowLayout->addWidget(stop);
		g.row->setLayout(rowLayout);
		groupsLayout->addWidget(g.row);

		const std::string n = name.toUtf8().constData();
		const std::string startName = "FijiSuiteStartGroup" + n;
		const std::string stopName = "FijiSuiteStopGroup" + n;
		const std::string startText = QString::fromUtf8(obs_module_text("StartGroup")).arg(name).toUtf8().constData();
		const std::string stopText = QString::fromUtf8(obs_module_text("StopGroup")).arg(name).toUtf8().constData();
		g.hotkey = obs_hotkey_pair_register_frontend(
			startName.c_str(), startText.c_str(), stopName.c_str(), stopText.c_str(),
			[](void *data, obs_hotkey_pair_id, obs_hotkey_t *, bool pressed) {
				if (!pressed || !output_dock) {
					return false;
				}
				const QString group = static_cast<GroupRow *>(data)->name;
				QMetaObject::invokeMethod(output_dock, [group] { output_dock->StartGroup(group); });
				return true;
			},
			[](void *data, obs_hotkey_pair_id, obs_hotkey_t *, bool pressed) {
				if (!pressed || !output_dock) {
					return false;
				}
				const QString group = static_cast<GroupRow *>(data)->name;
				QMetaObject::invokeMethod(output_dock, [group] { output_dock->StopGroup(group); });
				return true;
			},
			&g, &g);
		OBSDataAutoRelease h = hotkeys ? obs_data_get_obj(hotkeys, n.c_str()) : nullptr;
		if (h) {
			OBSDataArrayAutoRelease startKeys = obs_data_get_array(h, "start");
			OBSDataArrayAutoRelease stopKeys = obs_data_get_array(h, "stop");
			obs_hotkey_pair_load(g.hotkey, startKeys, stopKeys);
		}
	}
	UpdateGroupStatus();
}

void OutputDock::UpdateGroupStatus()
{
	for (auto &g : groupRows) {
		int members = 0;
		int live = 0;
		for (const auto &ow : outputWidgets) {
			if (ow->GetGroup() != g.name) {
				continue;
			}
			members++;
			if (ow->IsActive()) {
				live++;
			}
		}
		const auto text = QString::fromUtf8(obs_module_text("GroupStatus")).arg(g.name).arg(live).arg(members);
		if (g.label->text() != text) {
			g.label->setText(text);
		}
	}
}
