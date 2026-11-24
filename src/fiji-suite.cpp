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
#include "model/deck-model.hpp"
#include "model/profile-model.hpp"
#include "obs/profile-file.hpp"
#include "obs/obs-data-store.hpp"
#include "obs/obs-deck-hotkeys.hpp"
#include "obs/obs-deck-steps.hpp"
#include "obs/request-obs-api.hpp"
#include "state/link-state.hpp"
#include "utils/fiji-events.hpp"
#include "utils/obs-websocket-api.h"
#include "utils/update-check.hpp"
#include "obs/look-installer.hpp"
#include "view/restart-notice.hpp"
#include "version.h"
#include <obs-frontend-api.h>
#include <obs.hpp>
#include <obs-module.h>
#include <QApplication>
#include <QDockWidget>
#include <QMainWindow>
#include <QJsonDocument>
#include <QMap>
#include <QMenu>
#include <util/dstr.h>

OBS_DECLARE_MODULE()
OBS_MODULE_AUTHOR("darkzeromex");
OBS_MODULE_USE_DEFAULT_LOCALE("fiji-suite", "en-US")

// The single dock mode this fork uses; see the comment on modesTabBar in obs_module_load.
#define DOCK_MODE "Default"
// Per-profile configuration file.

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
// Themes installed for the first time when this OBS started (they show up after a restart).
static int themes_added = 0;

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
	// the canvases from the profile model, as a list of their own: the docks keep their item and report changes back
	// (fiji_publish_canvas); deleted ones are dropped and new uuids filled in below, and the result goes back at the end
	obs_data_array_t *canvas = nullptr;
	{
		OBSDataAutoRelease wrapper = obs_data_create_from_json(
			QJsonDocument(QJsonObject{{"canvas", fiji_profile() ? fiji_profile()->canvases() : QJsonArray()}})
				.toJson(QJsonDocument::Compact)
				.constData());
		canvas = obs_data_get_array(wrapper, "canvas");
	}
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
	if (auto profile = fiji_profile()) {
		profile->setCanvases(hotkey_json(canvas)); // (an obs_data array to JSON, like the hotkeys)
	}
	obs_data_array_release(canvas);
	for (const auto &it : canvas_clone_docks) {
		it->UpdateSettings(nullptr);
	}
	MultiviewDock::RebuildAll();
}

// Hotkey bindings between OBS (obs_data arrays) and the profile model (JSON), via a one-key wrapper object.
QJsonArray hotkey_json(obs_data_array_t *keys)
{
	OBSDataAutoRelease wrapper = obs_data_create();
	obs_data_set_array(wrapper, "keys", keys);
	return QJsonDocument::fromJson(QByteArray(obs_data_get_json(wrapper))).object().value("keys").toArray();
}

obs_data_array_t *hotkey_obs(const QJsonArray &keys)
{
	OBSDataAutoRelease wrapper = obs_data_create_from_json(
		QJsonDocument(QJsonObject{{"keys", keys}}).toJson(QJsonDocument::Compact).constData());
	return obs_data_get_array(wrapper, "keys");
}

QString fiji_view_string(const std::string &key)
{
	auto profile = fiji_profile();
	return profile ? profile->view(QString::fromStdString(key)).toString() : QString();
}

bool fiji_view_bool(const std::string &key, bool fallback)
{
	auto profile = fiji_profile();
	return profile ? profile->view(QString::fromStdString(key)).toBool(fallback) : fallback;
}

void fiji_set_view(const std::string &key, const QJsonValue &value)
{
	if (auto profile = fiji_profile()) {
		profile->setView(QString::fromStdString(key), value);
	}
}

void fiji_publish_canvas(obs_data_t *canvas_settings)
{
	if (auto profile = fiji_profile(); profile && canvas_settings) {
		profile->updateCanvas(QJsonDocument::fromJson(QByteArray(obs_data_get_json(canvas_settings))).object());
	}
}

void save_current_profile_config(bool save_docks)
{
	if (!current_profile_config) {
		return;
	}
	const QString file = fiji::obs::CurrentProfileFile();
	if (file.isEmpty()) {
		return;
	}
	if (save_docks) {
		save_dock_state(QStringLiteral(DOCK_MODE));
	}
	if (multiview_over_preview_hotkey != OBS_INVALID_HOTKEY_ID && fiji_profile()) {
		OBSDataArrayAutoRelease keys = obs_hotkey_save(multiview_over_preview_hotkey);
		fiji_profile()->setHotkey(QStringLiteral("multiview_over_preview_hotkey"), hotkey_json(keys));
	}
	if (output_dock) {
		output_dock->SaveSettings();
	}
	// the old settings object, with the parts the profile model owns on top (they are mirrored, so this is a check)
	auto document = QJsonDocument::fromJson(QByteArray(obs_data_get_json(current_profile_config))).object();
	if (auto profile = fiji_profile()) {
		const auto owned = profile->owned();
		for (auto it = owned.begin(); it != owned.end(); ++it) {
			document.insert(it.key(), it.value());
		}
	}
	if (!fiji::obs::SaveProfileFile(file, document)) {
		blog(LOG_WARNING, "[Fiji Suite] Failed to save configuration file");
	} else {
		blog(LOG_INFO, "[Fiji Suite] Saved configuration file");
	}
}

void load_current_profile_config()
{
	obs_data_release(current_profile_config);
	current_profile_config = nullptr;

	const QString file = fiji::obs::CurrentProfileFile();
	if (file.isEmpty()) {
		return;
	}
	const auto loaded = fiji::obs::LoadProfileFile(file);
	blog(loaded ? LOG_INFO : LOG_WARNING, loaded ? "[Fiji Suite] Loaded configuration file"
						     : "[Fiji Suite] No configuration file loaded");
	const auto [document, first_create] =
		fiji::model::withFirstRunDefaults(loaded.value_or(QJsonObject()), loaded.has_value(), fiji::obs::ProfileRecordPath());
	current_profile_config = obs_data_create_from_json(QJsonDocument(document).toJson(QJsonDocument::Compact).constData());
	if (auto profile = fiji_profile()) {
		profile->load(document);
	}
	load_canvas(first_create);

	load_outputs();

	if (auto profile = fiji_profile()) {
		OBSDataArrayAutoRelease overPreviewKeys =
			hotkey_obs(profile->hotkey(QStringLiteral("multiview_over_preview_hotkey")).toArray());
		obs_hotkey_load(multiview_over_preview_hotkey, overPreviewKeys);
	}

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

// The MVC + State side of the plugin (CLAUDE.md), built when the module loads, on the UI thread: the link state
// machine, the dispatcher that runs every bridge command on this thread, its OBS handler, the decks (model, steps,
// hotkeys, controller) and the optional bot link (started once OBS has finished loading).
namespace {
struct Core {
	fiji::state::LinkStateMachine link;
	fiji::controller::CommandDispatcher dispatcher;
	fiji::obs::RequestObsApi obs;
	fiji::controller::ObsCommandHandler handler{obs};
	fiji::obs::ObsDataStore store{config_folder(), [](const QString &line) {
		blog(LOG_WARNING, "[Fiji Suite] %s", line.toUtf8().constData());
	}};
	fiji::model::DeckModel deckModel{store};
	fiji::model::ProfileModel profile;
	fiji::obs::ObsDeckSteps deckSteps;
	fiji::obs::ObsDeckHotkeys deckHotkeys{&deckSteps};
	fiji::controller::DeckController decks{deckModel, deckSteps, &deckHotkeys, [](const QString &name, const QJsonObject &data) {
		if (!fiji_events_wanted()) {
			return;
		}
		OBSDataAutoRelease d = data.isEmpty() ? nullptr
						      : obs_data_create_from_json(QJsonDocument(data).toJson(QJsonDocument::Compact).constData());
		fiji_emit_event(name.toUtf8().constData(), d);
	}};
	fiji::bridge::BotLink bot{dispatcher, link, QStringLiteral(PROJECT_VERSION),
				  [](const QString &line) { blog(LOG_INFO, "[Fiji Suite] %s", line.toUtf8().constData()); }};
	QString mainScene; // for scene.program's "previous"

	Core()
	{
		dispatcher.registerHandler(&handler);
		dispatcher.registerHandler(&decks);
		// the parts of the profile the model owns, copied into the old settings object for code that reads it there
		QObject::connect(&profile, &fiji::model::ProfileModel::changed, [this] {
			if (!current_profile_config) {
				return;
			}
			OBSDataAutoRelease owned =
				obs_data_create_from_json(QJsonDocument(profile.owned()).toJson(QJsonDocument::Compact).constData());
			obs_data_apply(current_profile_config, owned);
			QStringList gone; // view state the model removed
			for (obs_data_item_t *item = obs_data_first(current_profile_config); item; obs_data_item_next(&item)) {
				const QString key = QString::fromUtf8(obs_data_item_get_name(item));
				if (fiji::model::ProfileModel::isViewKey(key) && profile.view(key).isUndefined()) {
					gone.append(key);
				}
			}
			for (const auto &key : gone) {
				obs_data_erase(current_profile_config, key.toUtf8().constData());
			}
		});
		fiji::bridge::BotLink::setInstance(&bot);
		decks.start();
	}

	static QString config_folder()
	{
		char *path = obs_module_config_path("");
		const QString folder = QString::fromUtf8(path);
		bfree(path);
		return folder;
	}
	~Core() { fiji::bridge::BotLink::setInstance(nullptr); }
};
Core *core = nullptr;

void start_core()
{
	if (core) {
		return;
	}
	core = new Core;
	OBSSourceAutoRelease scene = obs_frontend_get_current_scene();
	core->mainScene = scene ? QString::fromUtf8(obs_source_get_name(scene)) : QString();
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

fiji::model::ProfileModel *fiji_profile()
{
	return core ? &core->profile : nullptr;
}

fiji::controller::DeckController *fiji_decks()
{
	return core ? &core->decks : nullptr;
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
		if (themes_added > 0) { // OBS lists new themes only when it starts
			themes_added = 0;
			fiji::view::ShowRestartNotice(static_cast<QWidget *>(obs_frontend_get_main_window()),
						      QString::fromUtf8(obs_module_text("ThemesAddedTitle")),
						      QString::fromUtf8(obs_module_text("ThemesAddedText")));
		}
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
		if (core) {
			core->bot.shutdown();
			core->decks.shutdown();
		}
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
		const QString geom = fiji_view_string("config_geometry");
		if (!geom.isEmpty()) {
			configDialog->restoreGeometry(QByteArray::fromBase64(geom.toUtf8()));
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
			// which of OBS's own outputs the dock shows: the profile model owns these
			if (auto profile = fiji_profile()) {
				outputs_changed |= profile->setMainOutputs(
					{obs_data_get_bool(settings, "main_stream_output_show"),
					 obs_data_get_bool(settings, "main_record_output_show"),
					 obs_data_get_bool(settings, "main_backtrack_output_show"),
					 obs_data_get_bool(settings, "main_virtual_cam_output_show")});
			}
			// the canvases: the profile model owns the list
			const auto edited_canvases =
				QJsonDocument::fromJson(QByteArray(obs_data_get_json(settings))).object().value("canvas");
			if (auto profile = fiji_profile();
			    profile && edited_canvases.isArray() && profile->setCanvases(edited_canvases.toArray())) {
				canvas_changed = true;
			}
			// Fiji Suite's outputs: the profile model owns the list
			const auto edited = QJsonDocument::fromJson(QByteArray(obs_data_get_json(settings))).object().value("outputs");
			if (auto profile = fiji_profile(); profile && edited.isArray() && profile->setOutputs(edited.toArray())) {
				outputs_changed = true;
				check_new_canvas = true;
			}
			obs_data_release(settings);
		} else {
			canvas_changed = true;
			outputs_changed = true;
			current_profile_config = settings;
		}
		fiji_set_view("config_geometry", QString::fromUtf8(configDialog->saveGeometry().toBase64()));
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
	themes_added = fiji::obs::InstallLook();

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
	start_core();
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
	LocalServer::Stop();
	stop_core();
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
