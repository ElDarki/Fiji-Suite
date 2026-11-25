#include "dialogs/config-dialog.hpp"
#include "decks/local-server.hpp"
#include <QDesktopServices>
#include <QUrl>
#include "dialogs/name-dialog.hpp"
#include "docks/canvas-clone-dock.hpp"
#include "docks/canvas-dock.hpp"
#include "docks/capture-dock.hpp"
#include "docks/filters-dock.hpp"
#include "docks/live-scenes-dock.hpp"
#include "docks/multiview-dock.hpp"
#include "docks/output-dock.hpp"
#include "docks/properties-dock.hpp"
#include "docks/scenes-dock.hpp"
#include "docks/sources-dock.hpp"
#include "docks/stats-dock.hpp"
#include "docks/transform-dock.hpp"
#include "docks/transitions-dock.hpp"
#include "bridge/bot-link.hpp"
#include "controller/command-dispatcher.hpp"
#include "controller/deck-controller.hpp"
#include "controller/obs-command-handler.hpp"
#include "controller/output-controller.hpp"
#include "model/deck-model.hpp"
#include "obs/obs-data-store.hpp"
#include "obs/obs-deck-hotkeys.hpp"
#include "obs/obs-deck-steps.hpp"
#include "obs/request-obs-api.hpp"
#include "state/deck-keys.hpp"
#include "state/output-states.hpp"
#include "state/link-state.hpp"
#include "utils/fiji-events.hpp"
#include "utils/obs-websocket-api.h"
#include "utils/update-check.hpp"
#include "utils/fiji-look.hpp"
#include "version.h"
#include <obs-frontend-api.h>
#include <obs.hpp>
#include <obs-module.h>
#include <QApplication>
#include <QDockWidget>
#include <QMainWindow>
#include <QMap>
#include <QMenu>
#include <util/dstr.h>

OBS_DECLARE_MODULE()
OBS_MODULE_AUTHOR("darkzeromex");
OBS_MODULE_USE_DEFAULT_LOCALE("fiji-suite", "en-US")

// The single dock mode this fork uses; see the comment on modesTabBar in obs_module_load.
#define DOCK_MODE "Default"
// Per-profile configuration file.
#define CONFIG_FILE "fiji-suite.json"

obs_data_t *current_profile_config = nullptr;
QTabBar *modesTabBar = nullptr;


OBSBasicSettings *configDialog = nullptr;
OutputDock *output_dock = nullptr;
PropertiesDock *properties_dock = nullptr;
FiltersDock *filters_dock = nullptr;
TransformDock *transform_dock = nullptr;
LiveScenesDock *live_scenes_dock = nullptr;
CanvasDock *component_dock = nullptr;
StatsDock *stats_dock = nullptr;
ScenesDock *scenes_dock = nullptr;
SourcesDock *sources_dock = nullptr;
TransitionsDock *transitions_dock = nullptr;
MultiviewDock *multiview_dock = nullptr;
obs_hotkey_id multiview_over_preview_hotkey = OBS_INVALID_HOTKEY_ID;


extern std::list<CanvasDock *> canvas_docks;
extern std::list<CanvasCloneDock *> canvas_clone_docks;


static bool finished_loading = false;

void transition_start(void *, calldata_t *)
{
	QMetaObject::invokeMethod(live_scenes_dock, "MainSceneChanged", Qt::QueuedConnection);
	for (const auto &it : canvas_docks) {
		QMetaObject::invokeMethod(it, "MainSceneChanged", Qt::QueuedConnection);
	}
}

void save_dock_state(QString mode)
{
	if (mode.isEmpty() || !current_profile_config) {
		return;
	}
	for (const auto &it : canvas_docks) {
		QMetaObject::invokeMethod(it, "SaveSettings", Q_ARG(bool, false), Q_ARG(QString, mode));
	}
	for (const auto &it : canvas_clone_docks) {
		QMetaObject::invokeMethod(it, "SaveSettings", Q_ARG(bool, false), Q_ARG(QString, mode));
	}
	if (component_dock) {
		QMetaObject::invokeMethod(component_dock, "SaveSettings", Q_ARG(bool, false), Q_ARG(QString, mode));
	}
	if (stats_dock) {
		QMetaObject::invokeMethod(stats_dock, "SaveSettings", Q_ARG(bool, false), Q_ARG(QString, mode));
	}
}

static bool scene_collection_changing = false;

void load_dock_state(QString mode)
{
	if (!current_profile_config) {
		return;
	}
	scene_collection_changing = false;
	QList<QDockWidget *> visible_canvas_docks;
	for (const auto &it : canvas_docks) {
		auto dw = qobject_cast<QDockWidget *>(it->parentWidget());
		if (dw && dw->isVisible()) {
			visible_canvas_docks.append(dw);
		}
		QMetaObject::invokeMethod(it, "LoadMode", Qt::QueuedConnection, Q_ARG(QString, mode));
	}
	for (const auto &it : canvas_clone_docks) {
		auto dw = qobject_cast<QDockWidget *>(it->parentWidget());
		if (dw && dw->isVisible()) {
			visible_canvas_docks.append(dw);
		}
		QMetaObject::invokeMethod(it, "LoadMode", Qt::QueuedConnection, Q_ARG(QString, mode));
	}
	if (component_dock) {
		QMetaObject::invokeMethod(component_dock, "LoadMode", Qt::QueuedConnection, Q_ARG(QString, mode));
	}
	if (stats_dock) {
		QMetaObject::invokeMethod(stats_dock, "LoadMode", Qt::QueuedConnection, Q_ARG(QString, mode));
	}
	if (scenes_dock && !visible_canvas_docks.isEmpty()) {
		QMetaObject::invokeMethod(scenes_dock, "UpdateCanvasFromDockList",
					  Q_ARG(QList<QDockWidget *>, visible_canvas_docks));
	}
}

void load_outputs()
{
	QMetaObject::invokeMethod(
		output_dock,
		[] {
			if (output_dock) {
				output_dock->LoadSettings();
			}
		},
		Qt::QueuedConnection);
}

void load_canvas(bool check_new_canvas)
{
	auto main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	if (!main_window) {
		return;
	}
	auto canvas = obs_data_get_array(current_profile_config, "canvas");
	auto canvas_count = obs_data_array_count(canvas);
	for (size_t i = 0; i < canvas_count;) {
		obs_data_t *t = obs_data_array_item(canvas, i);
		if (!t) {
			i++;
			continue;
		}
		if (obs_data_get_bool(t, "delete")) {
			const char *canvas_name = obs_data_get_string(t, "name");
			obs_frontend_remove_dock(canvas_name);
			auto uuid = obs_data_get_string(t, "uuid");
			for (const auto &it : canvas_docks) {
				if (strcmp(obs_canvas_get_uuid(it->GetCanvas()), uuid) == 0) {
					obs_frontend_remove_dock(it->parentWidget()->objectName().toUtf8().constData());
					break;
				}
			}
			for (const auto &it : canvas_clone_docks) {
				if (strcmp(obs_canvas_get_uuid(it->GetCanvas()), uuid) == 0) {
					obs_frontend_remove_dock(it->parentWidget()->objectName().toUtf8().constData());
					break;
				}
			}
			obs_canvas_t *c = obs_get_canvas_by_uuid(uuid);
			if (c && obs_canvas_removed(c)) {
				obs_canvas_release(c);
				c = nullptr;
			}
			if (!c) {
				c = obs_get_canvas_by_name(canvas_name);
			}
			if (c && obs_canvas_removed(c)) {
				obs_canvas_release(c);
				c = nullptr;
			}
			if (c) {
				obs_frontend_remove_canvas(c);
				obs_canvas_remove(c);
				obs_canvas_release(c);
			}
			obs_data_array_erase(canvas, i);
			obs_data_release(t);
			canvas_count--;
		} else if (strcmp(obs_data_get_string(t, "type"), "clone") == 0) {
			auto uuid = obs_data_get_string(t, "uuid");
			auto name = obs_data_get_string(t, "name");
			for (const auto &it : canvas_docks) {
				if (strcmp(obs_canvas_get_uuid(it->GetCanvas()), uuid) == 0) {
					obs_frontend_remove_dock(it->parentWidget()->objectName().toUtf8().constData());
					check_new_canvas = false;
					break;
				}
			}
			CanvasCloneDock *ccd = nullptr;
			for (const auto &it : canvas_clone_docks) {
				auto cuuid = obs_canvas_get_uuid(it->GetCanvas());
				if (strcmp(cuuid, uuid) == 0) {
					if (obs_canvas_removed(it->GetCanvas()) ||
					    strcmp(it->parentWidget()->objectName().toUtf8().constData(), name) != 0) {
						// canvas name changed, remove old dock and create a new one
						obs_frontend_remove_dock(it->parentWidget()->objectName().toUtf8().constData());
						check_new_canvas = false;
					} else {
						ccd = it;
					}
					break;
				} else if (strcmp(it->parentWidget()->objectName().toUtf8().constData(), name) == 0) {
					if (obs_canvas_removed(it->GetCanvas())) {
						obs_frontend_remove_dock(name);
						check_new_canvas = false;
						break;
					}
					obs_data_set_string(t, "uuid", cuuid);
					ccd = it;
					break;
				}
			}
			if (ccd) {
				ccd->UpdateSettings(t);
			} else {
				ccd = new CanvasCloneDock(t, main_window);
				std::string title = name;
				if (obs_frontend_add_dock_by_id(name, title.c_str(), ccd)) {
					canvas_clone_docks.push_back(ccd);
					if (!obs_data_get_bool(t, "has_loaded")) {
						ccd->parentWidget()->show();
						obs_data_set_bool(t, "has_loaded", true);
					}
				} else {
					delete ccd;
				}
			}
			i++;
		} else {
			auto uuid = obs_data_get_string(t, "uuid");
			auto name = obs_data_get_string(t, "name");
			for (const auto &it : canvas_clone_docks) {
				if (strcmp(obs_canvas_get_uuid(it->GetCanvas()), uuid) == 0) {
					obs_frontend_remove_dock(it->parentWidget()->objectName().toUtf8().constData());
					check_new_canvas = false;
					break;
				}
			}
			CanvasDock *cd = nullptr;
			for (const auto &it : canvas_docks) {
				auto cuuid = obs_canvas_get_uuid(it->GetCanvas());
				if (strcmp(cuuid, uuid) == 0) {
					if (obs_canvas_removed(it->GetCanvas()) ||
					    strcmp(it->parentWidget()->objectName().toUtf8().constData(), name) != 0) {
						// canvas name changed, remove old dock and create a new one
						obs_frontend_remove_dock(it->parentWidget()->objectName().toUtf8().constData());
						check_new_canvas = false;
					} else {
						cd = it;
					}
					break;
				} else if (strcmp(it->parentWidget()->objectName().toUtf8().constData(), name) == 0) {
					if (obs_canvas_removed(it->GetCanvas())) {
						obs_frontend_remove_dock(name);
						check_new_canvas = false;
						break;
					}
					obs_data_set_string(t, "uuid", cuuid);
					cd = it;
					break;
				}
			}

			if (cd) {
				cd->UpdateSettings(t);
			} else {
				cd = new CanvasDock(t, main_window);
				std::string title = name;
				if (obs_frontend_add_dock_by_id(name, title.c_str(), cd)) {
					canvas_docks.push_back(cd);
					if (!obs_data_get_bool(t, "has_loaded")) {
						cd->parentWidget()->show();
						obs_data_set_bool(t, "has_loaded", true);
					}
				} else {
					delete cd;
				}
			}
			i++;
		}
	}
	obs_data_array_release(canvas);
	for (const auto &it : canvas_clone_docks) {
		it->UpdateSettings(nullptr);
	}
	MultiviewDock::RebuildAll();
}

void save_current_profile_config(bool save_docks)
{
	if (!current_profile_config) {
		return;
	}
	char *profile_path = obs_frontend_get_current_profile_path();
	if (!profile_path) {
		return;
	}

	struct dstr path;
	dstr_init_copy(&path, profile_path);
	bfree(profile_path);

	if (!dstr_is_empty(&path) && dstr_end(&path) != '/') {
		dstr_cat_ch(&path, '/');
	}
	dstr_cat(&path, CONFIG_FILE);

	if (save_docks) {
		save_dock_state(QStringLiteral(DOCK_MODE));
	}
	if (multiview_over_preview_hotkey != OBS_INVALID_HOTKEY_ID) {
		OBSDataArrayAutoRelease keys = obs_hotkey_save(multiview_over_preview_hotkey);
		obs_data_set_array(current_profile_config, "multiview_over_preview_hotkey", keys);
	}
	if (output_dock) {
		output_dock->SaveSettings();
	}

	if (!obs_data_save_json_safe(current_profile_config, path.array, "tmp", "bak")) {
		blog(LOG_WARNING, "[Fiji Suite] Failed to save configuration file");
	} else {
		blog(LOG_INFO, "[Fiji Suite] Saved configuration file");
	}

	dstr_free(&path);
}

void load_current_profile_config()
{
	obs_data_release(current_profile_config);
	current_profile_config = nullptr;

	char *profile_path = obs_frontend_get_current_profile_path();
	if (!profile_path) {
		return;
	}

	struct dstr path;
	dstr_init_copy(&path, profile_path);
	bfree(profile_path);

	if (!dstr_is_empty(&path) && dstr_end(&path) != '/') {
		dstr_cat_ch(&path, '/');
	}
	dstr_cat(&path, CONFIG_FILE);

	current_profile_config = obs_data_create_from_json_file_safe(path.array, "bak");
	dstr_free(&path);
	if (!current_profile_config) {
		current_profile_config = obs_data_create();
		obs_data_set_bool(current_profile_config, "main_stream_output_show", true);
		obs_data_set_bool(current_profile_config, "main_record_output_show", true);
		obs_data_set_bool(current_profile_config, "main_backtrack_output_show", true);
		obs_data_set_bool(current_profile_config, "main_virtual_cam_output_show", true);
		blog(LOG_WARNING, "[Fiji Suite] No configuration file loaded");
	} else {
		blog(LOG_INFO, "[Fiji Suite] Loaded configuration file");
	}

	bool first_create = false;
	auto canvas = obs_data_get_array(current_profile_config, "canvas");
	if (!canvas) {
		first_create = true;
		canvas = obs_data_array_create();
		const char *canvas_name = "Vertical";
		auto new_canvas = obs_data_create();
		obs_data_set_string(new_canvas, "name", canvas_name);
		obs_data_set_int(new_canvas, "color", 0x1F1A17);
		obs_data_array_push_back(canvas, new_canvas);
		obs_data_release(new_canvas);

		obs_data_set_array(current_profile_config, "canvas", canvas);

		auto outputs2 = obs_data_get_array(current_profile_config, "outputs");
		if (!outputs2) {
			outputs2 = obs_data_array_create();
			obs_data_set_array(current_profile_config, "outputs", outputs2);
		}
		if (obs_data_array_count(outputs2) < 1) {
			auto new_output = obs_data_create();
			obs_data_set_bool(new_output, "enabled", true);
			obs_data_set_string(new_output, "type", "stream");
			obs_data_set_string(new_output, "name", "Vertical Stream");
			obs_data_set_string(new_output, "canvas", canvas_name);
			obs_data_array_push_back(outputs2, new_output);
			obs_data_release(new_output);

			new_output = obs_data_create();
			obs_data_set_bool(new_output, "enabled", true);
			obs_data_set_string(new_output, "type", "backtrack");

			config_t *config = obs_frontend_get_profile_config();
			const char *mode = config_get_string(config, "Output", "Mode");
			const char *path = nullptr;
			if (mode && strcmp(mode, "Advanced") == 0) {
				path = config_get_string(config, "AdvOut", "RecFilePath");
			}
			if (!path || path[0] == '\0') {
				path = config_get_string(config, "SimpleOutput", "FilePath");
			}
			if (path) {
				obs_data_set_string(new_output, "path", path);
			}
			obs_data_set_string(new_output, "filename", "%CCYY-%MM-%DD %hh-%mm-%ss Vertical Backtrack");
			obs_data_set_string(new_output, "format", "hybrid_mp4");
			obs_data_set_string(new_output, "name", "Vertical Backtrack");
			obs_data_set_string(new_output, "canvas", canvas_name);
			obs_data_set_int(new_output, "max_time_sec", 10);
			obs_data_array_push_back(outputs2, new_output);
			obs_data_release(new_output);

			new_output = obs_data_create();
			obs_data_set_bool(new_output, "enabled", true);
			obs_data_set_string(new_output, "type", "virtual_cam");
			obs_data_set_string(new_output, "name", "Vertical Virtual Camera");
			obs_data_set_string(new_output, "canvas", canvas_name);
			obs_data_array_push_back(outputs2, new_output);
			obs_data_release(new_output);
		}
		obs_data_array_release(outputs2);
	} else {
		obs_data_array_release(canvas);
	}
	load_canvas(first_create);

	load_outputs();

	OBSDataArrayAutoRelease overPreviewKeys = obs_data_get_array(current_profile_config, "multiview_over_preview_hotkey");
	obs_hotkey_load(multiview_over_preview_hotkey, overPreviewKeys);

	QMetaObject::invokeMethod(
		modesTabBar, [] { load_dock_state(QStringLiteral(DOCK_MODE)); }, Qt::QueuedConnection);
}

static bool all_string_settings_the_same(obs_data_t *settings_a, obs_data_t *settings_b)
{
	size_t string_count = 0;
	obs_data_item_t *i = obs_data_first(settings_a);
	while (i) {
		const enum obs_data_type t = obs_data_item_gettype(i);
		if (t == OBS_DATA_STRING) {
			string_count++;
			const char *name = obs_data_item_get_name(i);
			const char *value_a = obs_data_item_get_string(i);
			const char *value_b = obs_data_get_string(settings_b, name);
			if (strcmp(value_a, value_b) != 0) {
				return false;
			}
		}
		obs_data_item_next(&i);
	}
	return string_count > 0;
}

static void log_same_sources()
{
	std::list<obs_source_t *> sources;
	obs_enum_sources(
		[](void *data, obs_source_t *source) {
			auto sources = static_cast<std::list<obs_source_t *> *>(data);
			auto id_a = obs_source_get_id(source);
			auto settings_a = obs_source_get_settings(source);
			bool do_not_duplicate = obs_source_get_output_flags(source) & OBS_SOURCE_DO_NOT_DUPLICATE;
			if (settings_a) {
				const char *json_a = obs_data_get_json(settings_a);
				for (auto &it : (*sources)) {
					auto id_b = obs_source_get_id(it);
					if (strcmp(id_a, id_b) == 0) {
						auto settings_b = obs_source_get_settings(it);
						if (settings_b) {
							const char *json_b = obs_data_get_json(settings_b);
							if (strcmp(json_a, json_b) == 0) {
								blog(LOG_WARNING,
								     "[Fiji Suite] Duplicate source found: '%s', '%s'",
								     obs_source_get_name(source), obs_source_get_name(it));
							} else if (do_not_duplicate &&
								   all_string_settings_the_same(settings_a, settings_b)) {
								blog(LOG_WARNING,
								     "[Fiji Suite] Similar source found: '%s', '%s'",
								     obs_source_get_name(source), obs_source_get_name(it));
							}
							obs_data_release(settings_b);
						}
					}
				}
				obs_data_release(settings_a);
			}
			sources->push_back(obs_source_get_ref(source));
			return true;
		},
		&sources);
	for (auto &it : sources) {
		obs_source_release(it);
	}
}

// The MVC + State side of the plugin (CLAUDE.md), built when the module loads: the dispatcher that runs every command
// on this (the UI) thread and its handlers (OBS, decks), the decks with their key states, store, steps and hotkeys,
// the link state machine and the optional bot link. The legacy code reaches it through fiji_command and the
// plugin's events.
namespace {
void emit_text_event(const char *event, const QString &text)
{
	if (!fiji_events_wanted()) {
		return;
	}
	OBSDataAutoRelease data = obs_data_create();
	obs_data_set_string(data, "text", text.toUtf8().constData());
	fiji_emit_event(event, data);
}

struct Core {
	fiji::state::LinkStateMachine link;
	fiji::controller::CommandDispatcher dispatcher;
	fiji::obs::RequestObsApi obs;
	fiji::controller::ObsCommandHandler handler{obs};
	fiji::state::OutputStates outputs;
	fiji::controller::OutputController outputController{obs, outputs};
	fiji::obs::ObsDataStore store{fiji::obs::ObsDataStore::configFolder()};
	fiji::model::DeckModel deckModel{store};
	fiji::state::DeckKeys keys;
	fiji::obs::ObsDeckSteps steps;
	fiji::obs::ObsDeckHotkeys hotkeys;
	fiji::controller::DeckController decks{deckModel, keys, steps, hotkeys};
	fiji::bridge::BotLink bot{dispatcher, link, QStringLiteral(PROJECT_VERSION),
				  [](const QString &line) { blog(LOG_INFO, "[Fiji Suite] %s", line.toUtf8().constData()); }};
	QString mainScene; // for scene.program's "previous"

	Core()
	{
		dispatcher.registerHandler(&handler);
		dispatcher.registerHandler(&decks);
		dispatcher.registerHandler(&outputController);
		fiji::bridge::BotLink::setInstance(&bot);
		QObject::connect(&outputs, &fiji::state::OutputStates::stateChanged,
				 [this](const QString &output, fiji::state::OutputStateId state, const QString &error) {
					 bot.acl().publishOutputState(output, fiji::state::outputStateName(state), error);
				 });
		hotkeys.onKey = [this](const QString &button, bool pressed) {
			dispatcher.post({pressed ? QStringLiteral("deck.press") : QStringLiteral("deck.release"),
					 {{"button", button}},
					 fiji::controller::CommandSource::Hotkey,
					 {}});
		};
		QObject::connect(&decks, &fiji::controller::DeckController::decksChanged,
				 [] { fiji_emit_event("decks_changed", nullptr); });
		QObject::connect(&decks, &fiji::controller::DeckController::keyChanged, [](const QString &button, bool running) {
			if (!fiji_events_wanted()) {
				return;
			}
			OBSDataAutoRelease data = obs_data_create();
			obs_data_set_string(data, "button", button.toUtf8().constData());
			obs_data_set_bool(data, "running", running);
			fiji_emit_event("button_state", data);
		});
		QObject::connect(&decks, &fiji::controller::DeckController::say,
				 [](const QString &text) { emit_text_event("say", text); });
		QObject::connect(&decks, &fiji::controller::DeckController::notice, [](const QString &text) {
			blog(LOG_INFO, "[Fiji Suite] Deck: %s", text.toUtf8().constData());
			emit_text_event("deck_notification", text);
		});
		decks.start();
	}
	~Core()
	{
		fiji::bridge::BotLink::setInstance(nullptr);
		decks.shutdown();
	}
};
Core *core = nullptr;

void start_core()
{
	if (core) {
		return;
	}
	core = new Core;
}

void stop_core()
{
	if (core) {
		core->bot.shutdown();
		delete core;
		core = nullptr;
	}
}

// OBS's own main canvas has no Fiji Suite dock to report it, so its changes are told to the bridge from here.
void main_canvas_event(enum obs_frontend_event event)
{
	if (!core || !core->bot.acl().linked()) {
		return;
	}
	if (event == OBS_FRONTEND_EVENT_SCENE_CHANGED) {
		OBSSourceAutoRelease scene = obs_frontend_get_current_scene();
		const auto name = scene ? QString::fromUtf8(obs_source_get_name(scene)) : QString();
		if (name != core->mainScene) {
			core->bot.acl().onPluginEvent(QStringLiteral("main_scene_changed"),
						      {{"scene", name}, {"previous", core->mainScene}});
			core->mainScene = name;
		}
	} else if (event == OBS_FRONTEND_EVENT_SCENE_LIST_CHANGED || event == OBS_FRONTEND_EVENT_TRANSITION_LIST_CHANGED) {
		core->bot.acl().onPluginEvent(QStringLiteral("scene_list_changed"), {});
	}
}
} // namespace

fiji::controller::CommandResult fiji_command(const fiji::controller::Command &command)
{
	if (!core) {
		fiji::controller::CommandResult r;
		r.ok = false;
		r.code = QStringLiteral("NOT_FOUND");
		r.message = QStringLiteral("Decks are not available");
		return r;
	}
	return core->dispatcher.call(command);
}

void fiji_core_event(const char *event, obs_data_t *data)
{
	if (!core || !data) {
		return;
	}
	core->outputs.onPluginEvent(QString::fromUtf8(event), QString::fromUtf8(obs_data_get_string(data, "output")),
				    QString::fromUtf8(obs_data_get_string(data, "last_error")));
}

fiji::state::LinkStateMachine *fiji_link_state()
{
	return core ? &core->link : nullptr;
}

void fiji_set_bot_address(const QString &address)
{
	if (core) {
		core->bot.setAddress(address);
	}
}

static void frontend_event(enum obs_frontend_event event, void *private_data)
{
	UNUSED_PARAMETER(private_data);
	if (event == OBS_FRONTEND_EVENT_SCENE_LIST_CHANGED || event == OBS_FRONTEND_EVENT_TRANSITION_LIST_CHANGED) {
		main_canvas_event(event);
	}
	if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING) {
		finished_loading = true;
		FijiLook::ShowRestartNotice(static_cast<QWidget *>(obs_frontend_get_main_window()));
		log_same_sources();
		struct obs_frontend_source_list transitions = {};
		obs_frontend_get_transitions(&transitions);
		for (size_t i = 0; i < transitions.sources.num; i++) {
			auto sh = obs_source_get_signal_handler(transitions.sources.array[i]);
			signal_handler_connect(sh, "transition_start", transition_start, nullptr);
		}
		obs_frontend_source_list_free(&transitions);

		load_current_profile_config();
		auto scene = obs_frontend_get_current_scene();
		if (scene) {
			if (properties_dock) {
				QMetaObject::invokeMethod(properties_dock, "SceneChanged", Qt::QueuedConnection,
							  Q_ARG(OBSSource, OBSSource(scene)));
			}
			obs_source_release(scene);
		}
		if (scenes_dock) {
			QMetaObject::invokeMethod(scenes_dock, "FinishedLoading", Qt::QueuedConnection);
		}
		LocalServer::Start();
		// The bot link runs whenever a bot address is set, independent of the remote control switch.
		// It starts here, not earlier: the bot may ask for OBS state as soon as it is connected.
		start_core();
		{
			OBSSourceAutoRelease scene = obs_frontend_get_current_scene();
			core->mainScene = scene ? QString::fromUtf8(obs_source_get_name(scene)) : QString();
		}
		if (auto user_config = obs_frontend_get_user_config()) {
			const char *address = config_get_string(user_config, "FijiSuite", "BotAddress");
			fiji_set_bot_address(QString::fromUtf8(address ? address : ""));
		}
		UpdateCheck::Start();
	} else if (event == OBS_FRONTEND_EVENT_PROFILE_CHANGED) {
		load_current_profile_config();
	} else if (event == OBS_FRONTEND_EVENT_PROFILE_CHANGING) {
		save_current_profile_config(true);
	} else if (event == OBS_FRONTEND_EVENT_EXIT || event == OBS_FRONTEND_EVENT_SCRIPTING_SHUTDOWN) {
		UpdateCheck::Stop();
		stop_core(); // the bot link, and the decks: every key and sound stops, the hotkeys go
		LocalServer::Stop();
		MultiviewDock::CloseWindows();
		MultiviewDock::ClearAll();
		if (current_profile_config) {
			obs_data_release(current_profile_config);
			current_profile_config = nullptr;
		}
		if (output_dock) {
			output_dock->Exiting();
		}
		if (properties_dock) {
			properties_dock->Exiting();
		}
	} else if (event == OBS_FRONTEND_EVENT_VIRTUALCAM_STARTED) {
		if (output_dock) {
			output_dock->UpdateMainVirtualCameraStatus(true);
		}
	} else if (event == OBS_FRONTEND_EVENT_VIRTUALCAM_STOPPED) {
		if (output_dock) {
			output_dock->UpdateMainVirtualCameraStatus(false);
		}
	} else if (event == OBS_FRONTEND_EVENT_STREAMING_STARTING || event == OBS_FRONTEND_EVENT_STREAMING_STARTED) {
		if (output_dock) {
			output_dock->UpdateMainStreamStatus(true);
		}
	} else if (event == OBS_FRONTEND_EVENT_STREAMING_STOPPING || event == OBS_FRONTEND_EVENT_STREAMING_STOPPED) {
		if (output_dock) {
			output_dock->UpdateMainStreamStatus(false);
		}
	} else if (event == OBS_FRONTEND_EVENT_RECORDING_STARTING || event == OBS_FRONTEND_EVENT_RECORDING_STARTED) {
		if (output_dock) {
			output_dock->UpdateMainRecordingStatus(true);
		}
	} else if (event == OBS_FRONTEND_EVENT_RECORDING_STOPPING || event == OBS_FRONTEND_EVENT_RECORDING_STOPPED) {
		if (output_dock) {
			output_dock->UpdateMainRecordingStatus(false);
		}
	} else if (event == OBS_FRONTEND_EVENT_REPLAY_BUFFER_STARTING || event == OBS_FRONTEND_EVENT_REPLAY_BUFFER_STARTED) {
		if (output_dock) {
			output_dock->UpdateMainBacktrackStatus(true);
		}
	} else if (event == OBS_FRONTEND_EVENT_REPLAY_BUFFER_STOPPING || event == OBS_FRONTEND_EVENT_REPLAY_BUFFER_STOPPED) {
		if (output_dock) {
			output_dock->UpdateMainBacktrackStatus(false);
		}
	} else if (event == OBS_FRONTEND_EVENT_SCENE_CHANGED || event == OBS_FRONTEND_EVENT_PREVIEW_SCENE_CHANGED) {
		main_canvas_event(event);
		if (live_scenes_dock) {
			QMetaObject::invokeMethod(live_scenes_dock, "MainSceneChanged", Qt::QueuedConnection);
		}
		if (scenes_dock) {
			QMetaObject::invokeMethod(scenes_dock, "MainSceneChanged", Qt::QueuedConnection);
		}
		for (const auto &it : canvas_docks) {
			QMetaObject::invokeMethod(it, "MainSceneChanged", Qt::QueuedConnection);
		}
	} else if (event == OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED) {
		if (finished_loading) {
			log_same_sources();
			struct obs_frontend_source_list transitions = {};
			obs_frontend_get_transitions(&transitions);
			for (size_t i = 0; i < transitions.sources.num; i++) {
				auto sh = obs_source_get_signal_handler(transitions.sources.array[i]);
				signal_handler_connect(sh, "transition_start", transition_start, nullptr);
			}
			obs_frontend_source_list_free(&transitions);
			load_current_profile_config();

			auto scene = obs_frontend_get_current_scene();
			if (scene) {
				if (properties_dock) {
					QMetaObject::invokeMethod(properties_dock, "SceneChanged", Qt::QueuedConnection,
								  Q_ARG(OBSSource, OBSSource(scene)));
				}
				obs_source_release(scene);
			}
		}
	} else if (event == OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGING) {
		scene_collection_changing = true;
	} else if (event == OBS_FRONTEND_EVENT_SCENE_COLLECTION_CLEANUP) {
		MultiviewDock::ClearAll();
		for (auto i = canvas_clone_docks.size(); i > 0; i--) {
			auto it = canvas_clone_docks.begin();
			std::advance(it, i - 1);
			auto dock = (*it)->parentWidget();
			if (dock) {
				obs_frontend_remove_dock(dock->objectName().toUtf8().constData());
			}
		}

		for (auto i = canvas_docks.size(); i > 0; i--) {
			auto it = canvas_docks.begin();
			std::advance(it, i - 1);
			auto dock = (*it)->parentWidget();
			if (dock) {
				obs_frontend_remove_dock(dock->objectName().toUtf8().constData());
			}
		}
	} else if (event == OBS_FRONTEND_EVENT_TRANSITION_DURATION_CHANGED) {
		if (transitions_dock) {
			QMetaObject::invokeMethod(transitions_dock, "TransitionDurationChanged", Qt::QueuedConnection);
		}
	} else if (event == OBS_FRONTEND_EVENT_TRANSITION_CHANGED) {
		if (transitions_dock) {
			QMetaObject::invokeMethod(transitions_dock, "TransitionChanged", Qt::QueuedConnection);
		}
	} else if (event == OBS_FRONTEND_EVENT_TRANSITION_LIST_CHANGED) {
		if (transitions_dock) {
			QMetaObject::invokeMethod(transitions_dock, "TransitionListChanged", Qt::QueuedConnection);
		}
	}
}

bool obs_data_array_equal(obs_data_array_t *a, obs_data_array_t *b)
{
	size_t a_count = obs_data_array_count(a);
	size_t b_count = obs_data_array_count(b);
	if (a_count != b_count) {
		return false;
	}
	for (size_t i = 0; i < a_count; i++) {
		obs_data_t *a_item = obs_data_array_item(a, i);
		obs_data_t *b_item = obs_data_array_item(b, i);
		const char *a_json = obs_data_get_json(a_item);
		const char *b_json = obs_data_get_json(b_item);
		bool equal = (strcmp(a_json, b_json) == 0);
		obs_data_release(a_item);
		obs_data_release(b_item);
		if (!equal) {
			return false;
		}
	}
	return true;
}

void open_config_dialog(int tab, const char *create_type)
{
	if (!configDialog) {
		configDialog = new OBSBasicSettings((QMainWindow *)obs_frontend_get_main_window());
		QObject::connect(configDialog, &OBSBasicSettings::accepted, [] {

		});
	}
	auto settings = obs_data_create();
	if (current_profile_config) {
		const char *geom = obs_data_get_string(current_profile_config, "config_geometry");
		if (geom && strlen(geom)) {
			QByteArray ba = QByteArray::fromBase64(QByteArray(geom));
			configDialog->restoreGeometry(ba);
		}
		obs_data_apply(settings, current_profile_config);
	}

	configDialog->LoadSettings(settings);
	if (tab > 0) {
		configDialog->ShowTab(tab);
	}
	configDialog->show();
	configDialog->SetCreateType(create_type);

	if (configDialog->exec() == QDialog::Accepted) {
		bool canvas_changed = false;
		bool outputs_changed = false;
		bool check_new_canvas = false;
		if (current_profile_config) {
			auto show = obs_data_get_bool(settings, "main_stream_output_show");
			if (show != obs_data_get_bool(current_profile_config, "main_stream_output_show")) {
				obs_data_set_bool(current_profile_config, "main_stream_output_show", show);
				outputs_changed = true;
			}
			show = obs_data_get_bool(settings, "main_record_output_show");
			if (show != obs_data_get_bool(current_profile_config, "main_record_output_show")) {
				obs_data_set_bool(current_profile_config, "main_record_output_show", show);
				outputs_changed = true;
			}
			show = obs_data_get_bool(settings, "main_backtrack_output_show");
			if (show != obs_data_get_bool(current_profile_config, "main_backtrack_output_show")) {
				obs_data_set_bool(current_profile_config, "main_backtrack_output_show", show);
				outputs_changed = true;
			}
			show = obs_data_get_bool(settings, "main_virtual_cam_output_show");
			if (show != obs_data_get_bool(current_profile_config, "main_virtual_cam_output_show")) {
				obs_data_set_bool(current_profile_config, "main_virtual_cam_output_show", show);
				outputs_changed = true;
			}
			obs_data_array_t *a = obs_data_get_array(current_profile_config, "canvas");
			obs_data_array_t *b = obs_data_get_array(settings, "canvas");
			if (!obs_data_array_equal(a, b)) {
				canvas_changed = true;
				obs_data_set_array(current_profile_config, "canvas", b);
			}
			obs_data_array_release(a);
			obs_data_array_release(b);
			a = obs_data_get_array(current_profile_config, "outputs");
			b = obs_data_get_array(settings, "outputs");
			if (!obs_data_array_equal(a, b)) {
				outputs_changed = true;
				check_new_canvas = true;
				obs_data_set_array(current_profile_config, "outputs", b);
			}
			obs_data_array_release(a);
			obs_data_array_release(b);
			obs_data_release(settings);
		} else {
			canvas_changed = true;
			outputs_changed = true;
			current_profile_config = settings;
		}
		obs_data_set_string(current_profile_config, "config_geometry", configDialog->saveGeometry().toBase64().constData());
		configDialog->SaveHotkeys();

		save_current_profile_config(true);
		if (canvas_changed) {
			load_canvas(check_new_canvas);
			if (fiji_events_wanted()) {
				fiji_emit_event("canvas_changed", nullptr);
			}
		}

		if (outputs_changed) {
			load_outputs();
			if (fiji_events_wanted()) {
				fiji_emit_event("outputs_changed", nullptr);
			}
		}
	} else {
		obs_data_release(settings);
	}
}
extern "C" const struct obs_source_info component_info;

bool obs_module_load(void)
{
	blog(LOG_INFO, "[Fiji Suite] loaded version %s", PROJECT_VERSION);

	obs_register_source(&component_info);

	// Her fonts, and the Fiji Sun / Fiji Moon themes copied where OBS looks for them.
	FijiLook::Install();

	obs_frontend_add_event_callback(frontend_event, nullptr);

	const auto main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());

	// Upstream keeps one window layout per "dock mode" and reads the current mode from this
	// tab bar. This fork has a single mode and never shows the bar: it leaves the OBS window
	// layout alone. The bar only exists so the canvas docks, the stats dock and the websocket
	// requests, which all ask it for the current mode, keep working unmodified.
	modesTabBar = new QTabBar();
	modesTabBar->addTab(QStringLiteral(DOCK_MODE));

	obs_frontend_add_tools_menu_item(
		obs_module_text("FijiSuiteSettings"), [](void *) { open_config_dialog(0, nullptr); }, nullptr);
	obs_frontend_add_tools_menu_item(
		obs_module_text("AddCanvas"), [](void *) { open_config_dialog(1, nullptr); }, nullptr);
	obs_frontend_add_tools_menu_item(
		obs_module_text("FijiSuiteMultiview"),
		[](void *) {
			if (multiview_dock && multiview_dock->parentWidget()) {
				multiview_dock->parentWidget()->show();
				multiview_dock->parentWidget()->raise();
			}
		},
		nullptr);
	obs_frontend_add_tools_menu_item(
		obs_module_text("OpenDecks"),
		[](void *) {
			if (auto server = LocalServer::Get()) {
				QDesktopServices::openUrl(QUrl(server->LocalAddress()));
			}
		},
		nullptr);
	obs_frontend_add_tools_menu_item(
		obs_module_text("MultiviewOverPreview"), [](void *) { MultiviewDock::ToggleOverMainPreview(); }, nullptr);
	multiview_over_preview_hotkey = obs_hotkey_register_frontend(
		"FijiSuiteMultiviewOverPreview", obs_module_text("MultiviewOverPreview"),
		[](void *, obs_hotkey_id, obs_hotkey_t *, bool pressed) {
			auto window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
			if (pressed && window) {
				QMetaObject::invokeMethod(window, [] { MultiviewDock::ToggleOverMainPreview(); }, Qt::QueuedConnection);
			}
		},
		nullptr);

	// Remote control is on unless switched off in the settings dialog; see obs_module_post_load.
	if (auto user_config = obs_frontend_get_user_config()) {
		config_set_default_bool(user_config, "FijiSuite", "RemoteControl", true);
		config_set_default_bool(user_config, "FijiSuite", "CheckForUpdates", true);
	}

	output_dock = new OutputDock(main_window);
	obs_frontend_add_dock_by_id("FijiSuiteOutput", obs_module_text("FijiSuiteOutput"), output_dock);
	properties_dock = new PropertiesDock(main_window);
	obs_frontend_add_dock_by_id("FijiSuiteProperties", obs_module_text("FijiSuiteProperties"), properties_dock);
	filters_dock = new FiltersDock(main_window);
	obs_frontend_add_dock_by_id("FijiSuiteFilters", obs_module_text("FijiSuiteFilters"), filters_dock);
	transform_dock = new TransformDock(main_window);
	obs_frontend_add_dock_by_id("FijiSuiteTransform", obs_module_text("FijiSuiteTransform"), transform_dock);
	live_scenes_dock = new LiveScenesDock(main_window);
	obs_frontend_add_dock_by_id("FijiSuiteLiveScenes", obs_module_text("FijiSuiteLiveScenes"), live_scenes_dock);
	auto capture_dock = new CaptureDock(main_window);
	obs_frontend_add_dock_by_id("FijiSuiteCapture", obs_module_text("FijiSuiteCapture"), capture_dock);
	stats_dock = new StatsDock(main_window);
	obs_frontend_add_dock_by_id("FijiSuiteStats", obs_module_text("FijiSuiteStats"), stats_dock);
	scenes_dock = new ScenesDock(main_window);
	obs_frontend_add_dock_by_id("FijiSuiteScenes", obs_module_text("FijiSuiteScenes"), scenes_dock);
	sources_dock = new SourcesDock(main_window);
	obs_frontend_add_dock_by_id("FijiSuiteSources", obs_module_text("FijiSuiteSources"), sources_dock);
	transitions_dock = new TransitionsDock(main_window);
	obs_frontend_add_dock_by_id("FijiSuiteTransitions", obs_module_text("FijiSuiteTransitions"),
				    transitions_dock);
	start_core(); // the decks register their hotkeys now, so OBS's settings show them
	multiview_dock = new MultiviewDock(main_window);
	obs_frontend_add_dock_by_id("FijiSuiteMultiview", obs_module_text("FijiSuiteMultiview"), multiview_dock);
	return true;
}

static void save_load(obs_data_t *save_data, bool saving, void *private_data)
{
	UNUSED_PARAMETER(save_data);
	UNUSED_PARAMETER(private_data);
	if (saving) {
		save_current_profile_config(!scene_collection_changing);
	}
}

void load_obs_websocket();

void obs_module_post_load()
{
	obs_frontend_add_save_callback(save_load, nullptr);

	// The vendor requests and events only exist when remote control is allowed.
	auto user_config = obs_frontend_get_user_config();
	if (!user_config || config_get_bool(user_config, "FijiSuite", "RemoteControl")) {
		load_obs_websocket();
	}
}

void unload_obs_websocket();

void obs_module_unload()
{
	UpdateCheck::Stop();
	stop_core();
	LocalServer::Stop();
	unload_obs_websocket();
	obs_frontend_remove_save_callback(save_load, nullptr);
	obs_frontend_remove_event_callback(frontend_event, nullptr);
	if (current_profile_config) {
		obs_data_release(current_profile_config);
		current_profile_config = nullptr;
	}

	obs_frontend_remove_dock("FijiSuiteOutput");
	obs_frontend_remove_dock("FijiSuiteProperties");
	obs_frontend_remove_dock("FijiSuiteFilters");
	obs_frontend_remove_dock("FijiSuiteTransform");
	obs_frontend_remove_dock("FijiSuiteLiveScenes");
	obs_frontend_remove_dock("FijiSuiteCapture");
	obs_frontend_remove_dock("FijiSuiteStats");
	obs_frontend_remove_dock("FijiSuiteScenes");
	obs_frontend_remove_dock("FijiSuiteSources");
	obs_frontend_remove_dock("FijiSuiteTransitions");
	MultiviewDock::CloseWindows();
	obs_frontend_remove_dock("FijiSuiteMultiview");
	if (multiview_over_preview_hotkey != OBS_INVALID_HOTKEY_ID) {
		obs_hotkey_unregister(multiview_over_preview_hotkey);
	}

	if (output_dock) {
		delete output_dock;
	}
	delete modesTabBar;
	modesTabBar = nullptr;
}

MODULE_EXPORT const char *obs_module_description(void)
{
	return obs_module_text("FijiSuite");
}

MODULE_EXPORT const char *obs_module_name(void)
{
	return obs_module_text("FijiSuite");
}

extern "C" void show_component_editor(const char *name);

void show_component_editor(const char *name)
{
	if (!component_dock) {
		return;
	}
	QMetaObject::invokeMethod(component_dock, "SwitchScene", Q_ARG(QString, QString::fromUtf8(name)));
	QMetaObject::invokeMethod(component_dock, [] {
		auto window = QApplication::activeWindow();
		if (window->objectName() == "OBSBasicProperties") {
			window->close();
		}
		component_dock->parentWidget()->show();
		component_dock->parentWidget()->raise();
		component_dock->parentWidget()->setFocus();
	});
}
