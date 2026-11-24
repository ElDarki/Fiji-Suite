
#include "../docks/canvas-clone-dock.hpp"
#include "../docks/canvas-dock.hpp"
#include "../docks/live-scenes-dock.hpp"
#include "../docks/output-dock.hpp"
#include "../version.h"
#include "../bridge/bot-link.hpp"
#include "../decks/local-server.hpp"
#include "../controller/deck-controller.hpp"
#include "fiji-events.hpp"
#include "obs-websocket-api.h"
#include <obs-frontend-api.h>
#include <obs.hpp>
#include <QDateTime>
#include <QJsonDocument>
#include <list>
#include <QDockWidget>
#include <QMainWindow>
#include <QThread>

obs_websocket_vendor vendor = nullptr;
bool local_server_has_listeners();

void fiji_emit_event(const char *event, obs_data_t *data)
{
	if (vendor) {
		obs_websocket_vendor_emit_event(vendor, event, data);
	}
	if (auto server = LocalServer::Get()) {
		server->Event(event, data);
	}
	// The bridge ACL turns the plugin's events into contract EVENTs (or SYNC_STATEs), on the UI thread.
	auto link = fiji::bridge::BotLink::instance();
	if (link && link->acl().linked()) {
		const auto json = data ? QJsonDocument::fromJson(QByteArray(obs_data_get_json(data))).object() : QJsonObject();
		const auto name = QString::fromUtf8(event);
		QMetaObject::invokeMethod(link, [link, name, json] { link->acl().onPluginEvent(name, json); });
	}
}

bool fiji_events_wanted()
{
	auto link = fiji::bridge::BotLink::instance();
	return vendor || (link && link->acl().linked()) || local_server_has_listeners();
}

// Whether the bot link is up, for the deck editor: Twitch triggers and chat steps only work while it is.
void vendor_request_get_link(obs_data_t *, obs_data_t *response_data, void *)
{
	auto link = fiji::bridge::BotLink::instance();
	obs_data_set_bool(response_data, "linked", link && link->linkedNow());
	obs_data_set_bool(response_data, "success", true);
}
extern std::list<CanvasDock *> canvas_docks;
extern std::list<CanvasCloneDock *> canvas_clone_docks;
extern OutputDock *output_dock;
extern LiveScenesDock *live_scenes_dock;

void vendor_request_version(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	UNUSED_PARAMETER(request_data);
	obs_data_set_string(response_data, "version", PROJECT_VERSION);
	obs_data_set_bool(response_data, "success", true);
}

// Which mood the deck pages should start in: Moon when OBS uses a dark theme, Sun otherwise.
void vendor_request_get_look(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	UNUSED_PARAMETER(request_data);
	obs_data_set_bool(response_data, "dark", obs_frontend_is_theme_dark());
	obs_data_set_bool(response_data, "success", true);
}

void vendor_request_get_canvas(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	UNUSED_PARAMETER(request_data);
	auto ca = obs_data_array_create();
	for (const auto &it : canvas_docks) {
		auto c = obs_data_create();
		obs_data_set_string(c, "type", "extra");
		obs_data_set_string(c, "name", obs_canvas_get_name(it->GetCanvas()));
		obs_data_set_string(c, "uuid", obs_canvas_get_uuid(it->GetCanvas()));
		obs_video_info ovi;
		if (obs_canvas_get_video_info(it->GetCanvas(), &ovi)) {
			obs_data_set_int(c, "width", ovi.base_width);
			obs_data_set_int(c, "height", ovi.base_height);
		}
		obs_data_array_push_back(ca, c);
		obs_data_release(c);
	}
	for (const auto &it : canvas_clone_docks) {
		auto c = obs_data_create();
		obs_data_set_string(c, "type", "clone");
		obs_data_set_string(c, "name", obs_canvas_get_name(it->GetCanvas()));
		obs_data_set_string(c, "uuid", obs_canvas_get_uuid(it->GetCanvas()));
		obs_video_info ovi;
		if (obs_canvas_get_video_info(it->GetCanvas(), &ovi)) {
			obs_data_set_int(c, "width", ovi.base_width);
			obs_data_set_int(c, "height", ovi.base_height);
		}
		obs_data_array_push_back(ca, c);
		obs_data_release(c);
	}
	obs_data_set_bool(response_data, "success", true);
	obs_data_set_array(response_data, "canvas", ca);
	obs_data_array_release(ca);
}

void vendor_request_switch_scene(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	const char *scene_name = obs_data_get_string(request_data, "scene");
	if (scene_name[0] == '\0') {
		obs_data_set_string(response_data, "error", "'scene' not set");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	const char *canvas_name = obs_data_get_string(request_data, "canvas");
	for (const auto &it : canvas_docks) {
		if (canvas_name[0] == '\0' || strcmp(obs_canvas_get_name(it->GetCanvas()), canvas_name) == 0 ||
		    strcmp(obs_canvas_get_uuid(it->GetCanvas()), canvas_name) == 0) {
			QMetaObject::invokeMethod(it, "SwitchScene", Q_ARG(QString, QString::fromUtf8(scene_name)));
		}
	}

	obs_data_set_bool(response_data, "success", true);
}

void vendor_request_current_scene(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	const char *canvas_name = obs_data_get_string(request_data, "canvas");
	if (canvas_name[0] == '\0') {
		obs_data_set_string(response_data, "error", "'canvas' not set");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	for (const auto &it : canvas_docks) {
		auto canvas = it->GetCanvas();
		if (strcmp(obs_canvas_get_name(canvas), canvas_name) != 0 &&
		    strcmp(obs_canvas_get_uuid(canvas), canvas_name) != 0) {
			continue;
		}

		auto source = obs_canvas_get_channel(canvas, 0);
		if (source && obs_source_get_type(source) == OBS_SOURCE_TYPE_TRANSITION) {
			obs_source_release(source);
			source = obs_transition_get_active_source(source);
		}
		if (source) {
			obs_data_set_string(response_data, "scene", obs_source_get_name(source));
			obs_data_set_string(response_data, "scene_uuid", obs_source_get_uuid(source));
			obs_source_release(source);
		} else {
			obs_data_set_string(response_data, "scene", "");
			obs_data_set_string(response_data, "scene_uuid", "");
		}
		obs_data_set_bool(response_data, "success", true);
		return;
	}
	obs_data_set_string(response_data, "error", "'canvas' not found");
	obs_data_set_bool(response_data, "success", false);
}

void vendor_request_get_scenes(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	const char *canvas_name = obs_data_get_string(request_data, "canvas");
	if (canvas_name[0] == '\0') {
		obs_data_set_string(response_data, "error", "'canvas' not set");
		obs_data_set_bool(response_data, "success", false);
		return;
	}

	for (const auto &it : canvas_docks) {
		auto canvas = it->GetCanvas();
		if (strcmp(obs_canvas_get_name(canvas), canvas_name) != 0 &&
		    strcmp(obs_canvas_get_uuid(canvas), canvas_name) != 0) {
			continue;
		}

		auto sa = obs_data_array_create();
		obs_canvas_enum_scenes(
			canvas,
			[](void *param, obs_source_t *scene) {
				auto a = (obs_data_array_t *)param;
				auto s = obs_data_create();
				obs_data_set_string(s, "name", obs_source_get_name(scene));
				obs_data_set_string(s, "uuid", obs_source_get_uuid(scene));
				obs_data_array_push_back(a, s);
				obs_data_release(s);
				return true;
			},
			sa);
		obs_data_set_bool(response_data, "success", true);
		obs_data_set_array(response_data, "scenes", sa);
		obs_data_array_release(sa);
		return;
	}
	obs_data_set_string(response_data, "error", "'canvas' not found");
	obs_data_set_bool(response_data, "success", false);
}

void vendor_request_get_outputs(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	UNUSED_PARAMETER(request_data);
	if (!output_dock) {
		obs_data_set_string(response_data, "error", "Output dock not available");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	auto oa = output_dock->GetOutputsArray();
	obs_data_set_bool(response_data, "success", true);
	obs_data_set_array(response_data, "outputs", oa);
	obs_data_array_release(oa);
}

void vendor_request_start_output(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	const char *output_name = obs_data_get_string(request_data, "output");
	if (output_name[0] == '\0') {
		obs_data_set_string(response_data, "error", "'output' not set");
		obs_data_set_bool(response_data, "success", false);
		return;
	}

	std::string startName = "FijiSuiteStartOutput";
	startName += output_name;

	struct find_hotkey {
		obs_hotkey_t *hotkey;
		const char *name;
	};
	find_hotkey t = {};
	t.name = startName.c_str();
	obs_enum_hotkeys(
		[](void *param, obs_hotkey_id id, obs_hotkey_t *key) {
			UNUSED_PARAMETER(id);
			const auto hp = (struct find_hotkey *)param;
			const auto hn = obs_hotkey_get_name(key);
			if (strcmp(hp->name, hn) == 0) {
				hp->hotkey = key;
			}
			return true;
		},
		&t);
	if (t.hotkey) {
		obs_hotkey_trigger_routed_callback(obs_hotkey_get_id(t.hotkey), true);
		obs_hotkey_trigger_routed_callback(obs_hotkey_get_id(t.hotkey), false);
		obs_data_set_bool(response_data, "success", true);
		return;
	}
	obs_data_set_string(response_data, "error", "'output' not found");
	obs_data_set_bool(response_data, "success", false);
}

void vendor_request_stop_output(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	const char *output_name = obs_data_get_string(request_data, "output");
	if (output_name[0] == '\0') {
		obs_data_set_string(response_data, "error", "'output' not set");
		obs_data_set_bool(response_data, "success", false);
		return;
	}

	std::string stopName = "FijiSuiteStopOutput";
	stopName += output_name;

	struct find_hotkey {
		obs_hotkey_t *hotkey;
		const char *name;
	};
	find_hotkey t = {};
	t.name = stopName.c_str();
	obs_enum_hotkeys(
		[](void *param, obs_hotkey_id id, obs_hotkey_t *key) {
			UNUSED_PARAMETER(id);
			const auto hp = (struct find_hotkey *)param;
			const auto hn = obs_hotkey_get_name(key);
			if (strcmp(hp->name, hn) == 0) {
				hp->hotkey = key;
			}
			return true;
		},
		&t);
	if (t.hotkey) {
		obs_hotkey_trigger_routed_callback(obs_hotkey_get_id(t.hotkey), true);
		obs_hotkey_trigger_routed_callback(obs_hotkey_get_id(t.hotkey), false);
		obs_data_set_bool(response_data, "success", true);
		return;
	}
	obs_data_set_string(response_data, "error", "'output' not found");
	obs_data_set_bool(response_data, "success", false);
}

void vendor_request_start_all_outputs(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	UNUSED_PARAMETER(request_data);
	if (!output_dock) {
		obs_data_set_string(response_data, "error", "Output dock not available");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	QMetaObject::invokeMethod(output_dock, "StartAll", Q_ARG(bool, false), Q_ARG(bool, false));
	obs_data_set_bool(response_data, "success", true);
}

void vendor_request_stop_all_outputs(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	UNUSED_PARAMETER(request_data);
	if (!output_dock) {
		obs_data_set_string(response_data, "error", "Output dock not available");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	QMetaObject::invokeMethod(output_dock, "StopAll", Q_ARG(bool, false), Q_ARG(bool, false));
	obs_data_set_bool(response_data, "success", true);
}

void vendor_request_start_all_streams(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	UNUSED_PARAMETER(request_data);
	if (!output_dock) {
		obs_data_set_string(response_data, "error", "Output dock not available");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	QMetaObject::invokeMethod(output_dock, "StartAll", Q_ARG(bool, true), Q_ARG(bool, false));
	obs_data_set_bool(response_data, "success", true);
}

void vendor_request_stop_all_streams(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	UNUSED_PARAMETER(request_data);
	if (!output_dock) {
		obs_data_set_string(response_data, "error", "Output dock not available");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	QMetaObject::invokeMethod(output_dock, "StopAll", Q_ARG(bool, true), Q_ARG(bool, false));
	obs_data_set_bool(response_data, "success", true);
}

void vendor_request_start_all_recordings(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	UNUSED_PARAMETER(request_data);
	if (!output_dock) {
		obs_data_set_string(response_data, "error", "Output dock not available");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	QMetaObject::invokeMethod(output_dock, "StartAll", Q_ARG(bool, false), Q_ARG(bool, true));
	obs_data_set_bool(response_data, "success", true);
}

void vendor_request_stop_all_recordings(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	UNUSED_PARAMETER(request_data);
	if (!output_dock) {
		obs_data_set_string(response_data, "error", "Output dock not available");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	QMetaObject::invokeMethod(output_dock, "StopAll", Q_ARG(bool, false), Q_ARG(bool, true));
	obs_data_set_bool(response_data, "success", true);
}

void vendor_request_save_backtrack(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	const char *output_name = obs_data_get_string(request_data, "output");
	if (output_name[0] == '\0') {
		obs_data_set_string(response_data, "error", "'output' not set");
		obs_data_set_bool(response_data, "success", false);
		return;
	}

	std::string saveName = "FijiSuiteSaveBacktrack";
	saveName += output_name;

	struct find_hotkey {
		obs_hotkey_t *hotkey;
		const char *name;
	};
	find_hotkey t = {};
	t.name = saveName.c_str();
	obs_enum_hotkeys(
		[](void *param, obs_hotkey_id id, obs_hotkey_t *key) {
			UNUSED_PARAMETER(id);
			const auto hp = (struct find_hotkey *)param;
			const auto hn = obs_hotkey_get_name(key);
			if (strcmp(hp->name, hn) == 0) {
				hp->hotkey = key;
			}
			return true;
		},
		&t);
	if (t.hotkey) {
		obs_hotkey_trigger_routed_callback(obs_hotkey_get_id(t.hotkey), true);
		obs_hotkey_trigger_routed_callback(obs_hotkey_get_id(t.hotkey), false);
		obs_data_set_bool(response_data, "success", true);
		return;
	}
	obs_data_set_string(response_data, "error", "'output' not found");
	obs_data_set_bool(response_data, "success", false);
}

void vendor_request_add_chapter(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	const char *output_name = obs_data_get_string(request_data, "output");
	if (output_name[0] == '\0') {
		obs_data_set_string(response_data, "error", "'output' not set");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	bool result = output_dock->AddChapterToOutput(output_name, obs_data_get_string(request_data, "chapter_name"));

	obs_data_set_bool(response_data, "success", result);
}

void vendor_request_get_docks(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	UNUSED_PARAMETER(request_data);
	auto da = obs_data_array_create();

	obs_queue_task(
		OBS_TASK_UI,
		[](void *param) {
			auto da = static_cast<obs_data_array_t *>(param);
			auto main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
			if (!main_window) {
				return;
			}
			auto docks = main_window->findChildren<QDockWidget *>();
			for (auto &dock : docks) {
				auto di = obs_data_create();
				obs_data_set_string(di, "name", dock->objectName().toUtf8().constData());
				obs_data_set_string(di, "title", dock->windowTitle().toUtf8().constData());
				obs_data_set_bool(di, "visible", dock->isVisible());
				obs_data_set_bool(di, "floating", dock->isFloating());
				obs_data_array_push_back(da, di);
				obs_data_release(di);
			}
		},
		da, true);
	obs_data_set_array(response_data, "docks", da);
	obs_data_array_release(da);
}

void vendor_request_dock_show(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	const char *dock_name = obs_data_get_string(request_data, "dock");
	if (dock_name[0] == '\0') {
		obs_data_set_string(response_data, "error", "'dock' not set");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	auto dn = QString::fromUtf8(dock_name);
	auto main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	if (!main_window) {
		obs_data_set_string(response_data, "error", "Main window not available");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	auto docks = main_window->findChildren<QDockWidget *>();
	for (auto &dock : docks) {
		if (dock->objectName() == dn) {
			QMetaObject::invokeMethod(dock, "show");
			obs_data_set_bool(response_data, "success", true);
			return;
		}
	}
	obs_data_set_string(response_data, "error", "'dock' not found");
	obs_data_set_bool(response_data, "success", false);
}

void vendor_request_dock_hide(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	const char *dock_name = obs_data_get_string(request_data, "dock");
	if (dock_name[0] == '\0') {
		obs_data_set_string(response_data, "error", "'dock' not set");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	auto dn = QString::fromUtf8(dock_name);
	auto main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	if (!main_window) {
		obs_data_set_string(response_data, "error", "Main window not available");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	auto docks = main_window->findChildren<QDockWidget *>();
	for (auto &dock : docks) {
		if (dock->objectName() == dn) {
			QMetaObject::invokeMethod(dock, [dock] {
				dock->hide();
			});

			obs_data_set_bool(response_data, "success", true);
			return;
		}
	}
	obs_data_set_string(response_data, "error", "'dock' not found");
	obs_data_set_bool(response_data, "success", false);
}

void vendor_request_get_live_scenes(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	UNUSED_PARAMETER(request_data);
	if (!live_scenes_dock) {
		obs_data_set_string(response_data, "error", "Live Scenes dock not available");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	auto sa = live_scenes_dock->GetLiveScenesArray();
	obs_data_set_bool(response_data, "success", true);
	obs_data_set_array(response_data, "live_scenes", sa);
	obs_data_array_release(sa);
}

void vendor_request_live_scenes_add(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	const char *scene_name = obs_data_get_string(request_data, "scene");
	if (scene_name[0] == '\0') {
		obs_data_set_string(response_data, "error", "'scene' not set");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	if (!live_scenes_dock) {
		obs_data_set_string(response_data, "error", "Live Scenes dock not available");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	auto sn = QString::fromUtf8(scene_name);
	QMetaObject::invokeMethod(live_scenes_dock, [sn] { live_scenes_dock->AddLiveScene(sn); });
	obs_data_set_bool(response_data, "success", true);
}

void vendor_request_live_scenes_remove(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	const char *scene_name = obs_data_get_string(request_data, "scene");
	if (scene_name[0] == '\0') {
		obs_data_set_string(response_data, "error", "'scene' not set");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	if (!live_scenes_dock) {
		obs_data_set_string(response_data, "error", "Live Scenes dock not available");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	auto sn = QString::fromUtf8(scene_name);
	QMetaObject::invokeMethod(live_scenes_dock, [sn] { live_scenes_dock->RemoveLiveScene(sn); });
	obs_data_set_bool(response_data, "success", true);
}

void vendor_request_dock_show_panel(obs_data_t *request_data, obs_data_t *response_data, void *)
{

	const char *canvas_name = obs_data_get_string(request_data, "canvas");
	if (canvas_name[0] == '\0') {
		obs_data_set_string(response_data, "error", "'canvas' not set");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	const char *panel_name = obs_data_get_string(request_data, "panel");
	if (panel_name[0] == '\0') {
		obs_data_set_string(response_data, "error", "'panel' not set");
		obs_data_set_bool(response_data, "success", false);
		return;
	}

	for (const auto &it : canvas_docks) {
		auto canvas = it->GetCanvas();
		if (strcmp(obs_canvas_get_name(canvas), canvas_name) != 0 &&
		    strcmp(obs_canvas_get_uuid(canvas), canvas_name) != 0) {
			continue;
		}
		auto pn = QString::fromUtf8(panel_name);
		QMetaObject::invokeMethod(it, [it, pn] { it->SetPanelVisible(pn, true); });
		obs_data_set_bool(response_data, "success", true);
		return;
	}
	for (const auto &it : canvas_clone_docks) {
		auto canvas = it->GetCanvas();
		if (strcmp(obs_canvas_get_name(canvas), canvas_name) != 0 &&
		    strcmp(obs_canvas_get_uuid(canvas), canvas_name) != 0) {
			continue;
		}
		auto pn = QString::fromUtf8(panel_name);
		QMetaObject::invokeMethod(it, [it, pn] { it->SetPanelVisible(pn, true); });
		obs_data_set_bool(response_data, "success", true);
		return;
	}

	obs_data_set_string(response_data, "error", "'canvas' not found");
	obs_data_set_bool(response_data, "success", false);
}

void vendor_request_dock_hide_panel(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	const char *canvas_name = obs_data_get_string(request_data, "canvas");
	if (canvas_name[0] == '\0') {
		obs_data_set_string(response_data, "error", "'canvas' not set");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	const char *panel_name = obs_data_get_string(request_data, "panel");
	if (panel_name[0] == '\0') {
		obs_data_set_string(response_data, "error", "'panel' not set");
		obs_data_set_bool(response_data, "success", false);
		return;
	}

	for (const auto &it : canvas_docks) {
		auto canvas = it->GetCanvas();
		if (strcmp(obs_canvas_get_name(canvas), canvas_name) != 0 &&
		    strcmp(obs_canvas_get_uuid(canvas), canvas_name) != 0) {
			continue;
		}
		auto pn = QString::fromUtf8(panel_name);
		QMetaObject::invokeMethod(it, [it, pn] { it->SetPanelVisible(pn, false); });
		obs_data_set_bool(response_data, "success", true);
		return;
	}
	for (const auto &it : canvas_clone_docks) {
		auto canvas = it->GetCanvas();
		if (strcmp(obs_canvas_get_name(canvas), canvas_name) != 0 &&
		    strcmp(obs_canvas_get_uuid(canvas), canvas_name) != 0) {
			continue;
		}
		auto pn = QString::fromUtf8(panel_name);
		QMetaObject::invokeMethod(it, [it, pn] { it->SetPanelVisible(pn, false); });
		obs_data_set_bool(response_data, "success", true);
		return;
	}

	obs_data_set_string(response_data, "error", "'canvas' not found");
	obs_data_set_bool(response_data, "success", false);
}

void vendor_request_get_transitions(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	UNUSED_PARAMETER(request_data);
	const char *canvas_name = obs_data_get_string(request_data, "canvas");
	if (canvas_name[0] == '\0') {
		obs_data_set_string(response_data, "error", "'canvas' not set");
		obs_data_set_bool(response_data, "success", false);
		return;
	}

	for (const auto &it : canvas_docks) {
		auto canvas = it->GetCanvas();
		if (strcmp(obs_canvas_get_name(canvas), canvas_name) != 0 &&
		    strcmp(obs_canvas_get_uuid(canvas), canvas_name) != 0) {
			continue;
		}

		auto ta = obs_data_array_create();
		auto transitions = it->GetTransitions();
		for (const auto &t : transitions) {
			auto tr = obs_data_create();
			obs_data_set_string(tr, "name", obs_source_get_name(t));
			obs_data_set_string(tr, "uuid", obs_source_get_uuid(t));
			obs_data_set_string(tr, "type", obs_source_get_id(t));
			auto settings = obs_source_get_settings(t);
			if (settings) {
				obs_data_set_obj(tr, "settings", settings);
				obs_data_release(settings);
			}
			obs_data_array_push_back(ta, tr);
			obs_data_release(tr);
		}
		obs_data_set_bool(response_data, "success", true);
		obs_data_set_array(response_data, "transitions", ta);
		obs_data_array_release(ta);
		return;
	}
	obs_data_set_string(response_data, "error", "'canvas' not found");
	obs_data_set_bool(response_data, "success", false);
}

void vendor_request_switch_transition(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	const char *canvas_name = obs_data_get_string(request_data, "canvas");
	if (canvas_name[0] == '\0') {
		obs_data_set_string(response_data, "error", "'canvas' not set");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	const char *transition_name = obs_data_get_string(request_data, "transition");
	if (transition_name[0] == '\0') {
		obs_data_set_string(response_data, "error", "'transition' not set");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	for (const auto &it : canvas_docks) {
		auto canvas = it->GetCanvas();
		if (strcmp(obs_canvas_get_name(canvas), canvas_name) != 0 &&
		    strcmp(obs_canvas_get_uuid(canvas), canvas_name) != 0) {
			continue;
		}

		it->SetSelectedTransition(QString::fromUtf8(transition_name));

		obs_data_set_bool(response_data, "success", true);
		return;
	}
	obs_data_set_string(response_data, "error", "'canvas' not found");
	obs_data_set_bool(response_data, "success", false);
}

void vendor_request_transitions_add(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	const char *canvas_name = obs_data_get_string(request_data, "canvas");
	if (canvas_name[0] == '\0') {
		obs_data_set_string(response_data, "error", "'canvas' not set");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	const char *transition_name = obs_data_get_string(request_data, "name");
	if (transition_name[0] == '\0') {
		obs_data_set_string(response_data, "error", "'name' not set");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	const char *transition_type = obs_data_get_string(request_data, "type");
	if (transition_type[0] == '\0') {
		obs_data_set_string(response_data, "error", "'type' not set");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	obs_data_t *settings = obs_data_get_obj(request_data, "settings");
	for (const auto &it : canvas_docks) {
		auto canvas = it->GetCanvas();
		if (strcmp(obs_canvas_get_name(canvas), canvas_name) != 0 &&
		    strcmp(obs_canvas_get_uuid(canvas), canvas_name) != 0) {
			continue;
		}
		it->AddTransition(transition_type, transition_name, settings);
		obs_data_set_bool(response_data, "success", true);
		obs_data_release(settings);
		return;
	}
	obs_data_release(settings);
	obs_data_set_string(response_data, "error", "'canvas' not found");
	obs_data_set_bool(response_data, "success", false);
}

void vendor_request_transitions_remove(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	const char *canvas_name = obs_data_get_string(request_data, "canvas");
	if (canvas_name[0] == '\0') {
		obs_data_set_string(response_data, "error", "'canvas' not set");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	const char *transition_name = obs_data_get_string(request_data, "name");
	if (transition_name[0] == '\0') {
		obs_data_set_string(response_data, "error", "'name' not set");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	for (const auto &it : canvas_docks) {
		auto canvas = it->GetCanvas();
		if (strcmp(obs_canvas_get_name(canvas), canvas_name) != 0 &&
		    strcmp(obs_canvas_get_uuid(canvas), canvas_name) != 0) {
			continue;
		}
		it->RemoveTransition(transition_name);
		obs_data_set_bool(response_data, "success", true);
		return;
	}
	obs_data_set_string(response_data, "error", "'canvas' not found");
	obs_data_set_bool(response_data, "success", false);
}

// Runs on the UI thread and waits for it, so a request can read dock state safely.
static void run_on_ui(QObject *target, const std::function<void()> &fn)
{
	if (QThread::currentThread() == target->thread()) {
		fn();
	} else {
		QMetaObject::invokeMethod(target, fn, Qt::BlockingQueuedConnection);
	}
}

// Finds a canvas dock by canvas name or uuid. Sets the error on the response when there is none.
static CanvasDock *find_canvas_dock(obs_data_t *request_data, obs_data_t *response_data)
{
	const char *canvas_name = obs_data_get_string(request_data, "canvas");
	if (canvas_name[0] == '\0') {
		obs_data_set_string(response_data, "error", "'canvas' not set");
		obs_data_set_bool(response_data, "success", false);
		return nullptr;
	}
	for (const auto &it : canvas_docks) {
		auto canvas = it->GetCanvas();
		if (strcmp(obs_canvas_get_name(canvas), canvas_name) == 0 || strcmp(obs_canvas_get_uuid(canvas), canvas_name) == 0) {
			return it;
		}
	}
	obs_data_set_string(response_data, "error", "'canvas' not found");
	obs_data_set_bool(response_data, "success", false);
	return nullptr;
}

void vendor_request_get_studio_mode(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	auto dock = find_canvas_dock(request_data, response_data);
	if (!dock) {
		return;
	}
	run_on_ui(dock, [dock, response_data] {
		obs_data_set_bool(response_data, "enabled", dock->IsStudioMode());
		obs_data_set_string(response_data, "program_scene", dock->GetProgramSceneName().toUtf8().constData());
		obs_data_set_string(response_data, "preview_scene", dock->GetPreviewSceneName().toUtf8().constData());
	});
	obs_data_set_bool(response_data, "success", true);
}

void vendor_request_set_studio_mode(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	auto dock = find_canvas_dock(request_data, response_data);
	if (!dock) {
		return;
	}
	QMetaObject::invokeMethod(dock, "SetStudioMode", Q_ARG(bool, obs_data_get_bool(request_data, "enabled")));
	obs_data_set_bool(response_data, "success", true);
}

void vendor_request_set_preview_scene(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	const char *scene_name = obs_data_get_string(request_data, "scene");
	if (scene_name[0] == '\0') {
		obs_data_set_string(response_data, "error", "'scene' not set");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	auto dock = find_canvas_dock(request_data, response_data);
	if (!dock) {
		return;
	}
	QMetaObject::invokeMethod(dock, "SelectScene", Q_ARG(QString, QString::fromUtf8(scene_name)));
	obs_data_set_bool(response_data, "success", true);
}

void vendor_request_transition(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	auto dock = find_canvas_dock(request_data, response_data);
	if (!dock) {
		return;
	}
	QMetaObject::invokeMethod(dock, "TransitionToProgram");
	obs_data_set_bool(response_data, "success", true);
}

void vendor_request_show_multiview(obs_data_t *, obs_data_t *response_data, void *)
{
	auto main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	auto dock = main_window ? main_window->findChild<QDockWidget *>(QStringLiteral("FijiSuiteMultiview")) : nullptr;
	if (!dock) {
		obs_data_set_string(response_data, "error", "Multiview not available");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	QMetaObject::invokeMethod(dock, [dock] {
		dock->show();
		dock->raise();
	});
	obs_data_set_bool(response_data, "success", true);
}

void vendor_request_get_groups(obs_data_t *, obs_data_t *response_data, void *)
{
	if (!output_dock) {
		obs_data_set_string(response_data, "error", "Output dock not available");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	OBSDataArrayAutoRelease groups = obs_data_array_create();
	run_on_ui(output_dock, [&groups] {
		const auto states = output_dock->GetOutputStates();
		for (const auto &name : output_dock->GetGroups()) {
			OBSDataAutoRelease group = obs_data_create();
			OBSDataArrayAutoRelease outputs = obs_data_array_create();
			int live = 0;
			for (const auto &s : states) {
				if (s.main || s.group != name) {
					continue;
				}
				OBSDataAutoRelease output = obs_data_create();
				obs_data_set_string(output, "name", s.name.toUtf8().constData());
				obs_data_set_bool(output, "active", s.active);
				obs_data_array_push_back(outputs, output);
				if (s.active) {
					live++;
				}
			}
			obs_data_set_string(group, "name", name.toUtf8().constData());
			obs_data_set_array(group, "outputs", outputs);
			obs_data_set_int(group, "live", live);
			obs_data_array_push_back(groups, group);
		}
	});
	obs_data_set_array(response_data, "groups", groups);
	obs_data_set_bool(response_data, "success", true);
}

static void group_request(obs_data_t *request_data, obs_data_t *response_data, const char *slot)
{
	const char *group = obs_data_get_string(request_data, "group");
	if (group[0] == '\0') {
		obs_data_set_string(response_data, "error", "'group' not set");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	if (!output_dock) {
		obs_data_set_string(response_data, "error", "Output dock not available");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	QMetaObject::invokeMethod(output_dock, slot, Q_ARG(QString, QString::fromUtf8(group)));
	obs_data_set_bool(response_data, "success", true);
}

void vendor_request_start_group(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	group_request(request_data, response_data, "StartGroup");
}

void vendor_request_stop_group(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	group_request(request_data, response_data, "StopGroup");
}

// Everything the bot needs to describe the stream, in one call.
void vendor_request_get_status(obs_data_t *, obs_data_t *response_data, void *)
{
	OBSSourceAutoRelease current = obs_frontend_get_current_scene();
	obs_data_set_string(response_data, "main_scene", current ? obs_source_get_name(current) : "");
	OBSDataArrayAutoRelease mainScenes = obs_data_array_create();
	char **names = obs_frontend_get_scene_names();
	for (char **name = names; name && *name; name++) {
		OBSDataAutoRelease s = obs_data_create();
		obs_data_set_string(s, "name", *name);
		obs_data_array_push_back(mainScenes, s);
	}
	bfree(names);
	obs_data_set_array(response_data, "main_scenes", mainScenes);

	OBSDataArrayAutoRelease canvases = obs_data_array_create();
	OBSDataArrayAutoRelease outputs = obs_data_array_create();
	auto main_window = static_cast<QObject *>(obs_frontend_get_main_window());
	if (main_window) {
		run_on_ui(main_window, [&canvases, &outputs] {
			for (const auto &dock : canvas_docks) {
				OBSDataAutoRelease c = obs_data_create();
				obs_data_set_string(c, "name", obs_canvas_get_name(dock->GetCanvas()));
				obs_data_set_string(c, "scene", dock->GetProgramSceneName().toUtf8().constData());
				obs_data_set_bool(c, "studio_mode", dock->IsStudioMode());
				obs_data_set_string(c, "preview_scene", dock->GetPreviewSceneName().toUtf8().constData());
				OBSDataArrayAutoRelease scenes = obs_data_array_create();
				obs_canvas_enum_scenes(
					dock->GetCanvas(),
					[](void *param, obs_source_t *scene) {
						OBSDataAutoRelease s = obs_data_create();
						obs_data_set_string(s, "name", obs_source_get_name(scene));
						obs_data_array_push_back((obs_data_array_t *)param, s);
						return true;
					},
					scenes.Get());
				obs_data_set_array(c, "scenes", scenes);
				obs_data_array_push_back(canvases, c);
			}
			if (!output_dock) {
				return;
			}
			const auto now = QDateTime::currentDateTime();
			for (const auto &s : output_dock->GetOutputStates()) {
				OBSDataAutoRelease o = obs_data_create();
				obs_data_set_string(o, "name", s.name.toUtf8().constData());
				obs_data_set_string(o, "type", s.type.toUtf8().constData());
				obs_data_set_string(o, "canvas", s.canvas.toUtf8().constData());
				obs_data_set_string(o, "group", s.group.toUtf8().constData());
				obs_data_set_bool(o, "main", s.main);
				obs_data_set_bool(o, "active", s.active);
				obs_data_set_bool(o, "reconnecting", s.reconnecting);
				obs_data_set_int(o, "uptime", s.active && s.startTime.isValid() ? s.startTime.secsTo(now) : 0);
				obs_data_set_int(o, "total_frames", s.totalFrames);
				obs_data_set_int(o, "dropped_frames", s.droppedFrames);
				obs_data_set_int(o, "total_bytes", (long long)s.totalBytes);
				obs_data_array_push_back(outputs, o);
			}
		});
	}
	obs_data_set_array(response_data, "canvases", canvases);
	obs_data_set_array(response_data, "outputs", outputs);
	obs_data_set_bool(response_data, "success", true);
}

void vendor_request_set_main_scene(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	OBSSourceAutoRelease scene = obs_get_source_by_name(obs_data_get_string(request_data, "scene"));
	if (!scene || !obs_source_is_scene(scene)) {
		obs_data_set_string(response_data, "error", "'scene' not found");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	obs_frontend_set_current_scene(scene);
	obs_data_set_bool(response_data, "success", true);
}

// OBS's own outputs, by the same type names get_status reports for them.
static void main_output_request(obs_data_t *request_data, obs_data_t *response_data, bool start)
{
	const char *output = obs_data_get_string(request_data, "output");
	if (strcmp(output, "stream") == 0) {
		start ? obs_frontend_streaming_start() : obs_frontend_streaming_stop();
	} else if (strcmp(output, "record") == 0) {
		start ? obs_frontend_recording_start() : obs_frontend_recording_stop();
	} else if (strcmp(output, "backtrack") == 0) {
		start ? obs_frontend_replay_buffer_start() : obs_frontend_replay_buffer_stop();
	} else if (strcmp(output, "virtual_cam") == 0) {
		start ? obs_frontend_start_virtualcam() : obs_frontend_stop_virtualcam();
	} else {
		obs_data_set_string(response_data, "error", "'output' must be stream, record, backtrack or virtual_cam");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	obs_data_set_bool(response_data, "success", true);
}

void vendor_request_start_main(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	main_output_request(request_data, response_data, true);
}

void vendor_request_stop_main(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	main_output_request(request_data, response_data, false);
}


// Serves the deck pages' files (src/decks/local-server.cpp).
void vendor_request_get_web_file(obs_data_t *request_data, obs_data_t *response_data, void *);

static OBSDataAutoRelease deck_to_obs(const QJsonObject &object)
{
	return obs_data_create_from_json(QJsonDocument(object).toJson(QJsonDocument::Compact).constData());
}

static QJsonObject deck_to_json(obs_data_t *data)
{
	return data ? QJsonDocument::fromJson(QByteArray(obs_data_get_json(data))).object() : QJsonObject();
}

// ---- deck requests (docs/websocket.md): the deck pages, obs-websocket clients and deck "request" steps.
// Requests arrive on the caller's thread; the deck controller lives on the UI thread.
static bool on_decks(obs_data_t *response, const std::function<void(fiji::controller::DeckController *)> &fn)
{
	auto manager = fiji_decks();
	if (!manager) {
		obs_data_set_string(response, "error", "Decks are not available");
		obs_data_set_bool(response, "success", false);
		return false;
	}
	if (QThread::currentThread() == manager->thread()) {
		fn(manager);
	} else {
		QMetaObject::invokeMethod(manager, [&fn, manager] { fn(manager); }, Qt::BlockingQueuedConnection);
	}
	return true;
}

static void put_array(obs_data_t *response, const char *key, const QJsonArray &array)
{
	OBSDataAutoRelease wrapper = deck_to_obs(QJsonObject{{QLatin1String(key), array}});
	OBSDataArrayAutoRelease value = obs_data_get_array(wrapper, key);
	obs_data_set_array(response, key, value);
}

void vendor_request_get_decks(obs_data_t *, obs_data_t *response, void *)
{
	if (on_decks(response, [response](fiji::controller::DeckController *m) { put_array(response, "decks", m->decks()); })) {
		obs_data_set_bool(response, "success", true);
	}
}

void vendor_request_save_deck(obs_data_t *request, obs_data_t *response, void *)
{
	OBSDataAutoRelease deck = obs_data_get_obj(request, "deck");
	if (!deck) {
		obs_data_set_string(response, "error", "'deck' not set");
		obs_data_set_bool(response, "success", false);
		return;
	}
	const QJsonObject wanted = deck_to_json(deck);
	if (on_decks(response, [&](fiji::controller::DeckController *m) {
		    OBSDataAutoRelease saved = deck_to_obs(m->saveDeck(wanted));
		    obs_data_set_obj(response, "deck", saved);
	    })) {
		obs_data_set_bool(response, "success", true);
	}
}

void vendor_request_delete_deck(obs_data_t *request, obs_data_t *response, void *)
{
	const QString id = QString::fromUtf8(obs_data_get_string(request, "id"));
	bool found = false;
	if (!on_decks(response, [&](fiji::controller::DeckController *m) { found = m->deleteDeck(id); })) {
		return;
	}
	if (!found) {
		obs_data_set_string(response, "error", "'id' not found");
	}
	obs_data_set_bool(response, "success", found);
}

// press_button and fire_trigger are the same thing: "data" becomes the run's variables.
void vendor_request_press_button(obs_data_t *request, obs_data_t *response, void *)
{
	const QString button = QString::fromUtf8(obs_data_get_string(request, "button"));
	OBSDataAutoRelease data = obs_data_get_obj(request, "data");
	decks::Variables trigger;
	const QJsonObject values = deck_to_json(data);
	for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
		trigger.insert(it.key(), it.value().toVariant());
	}
	bool started = false;
	if (!on_decks(response, [&](fiji::controller::DeckController *m) { started = m->press(button, trigger); })) {
		return;
	}
	if (!started) {
		obs_data_set_string(response, "error", "no such button, or it is already running");
	}
	obs_data_set_bool(response, "success", started);
}

void vendor_request_release_button(obs_data_t *request, obs_data_t *response, void *)
{
	const QString button = QString::fromUtf8(obs_data_get_string(request, "button"));
	if (on_decks(response, [&](fiji::controller::DeckController *m) { m->release(button); })) {
		obs_data_set_bool(response, "success", true);
	}
}

void vendor_request_stop_button(obs_data_t *request, obs_data_t *response, void *)
{
	const QString button = QString::fromUtf8(obs_data_get_string(request, "button"));
	if (button.isEmpty()) {
		obs_data_set_string(response, "error", "'button' not set (use \"all\" to stop everything)");
		obs_data_set_bool(response, "success", false);
		return;
	}
	if (on_decks(response, [&](fiji::controller::DeckController *m) { m->stop(button); })) {
		obs_data_set_bool(response, "success", true);
	}
}

void vendor_request_get_deck_state(obs_data_t *, obs_data_t *response, void *)
{
	if (on_decks(response, [response](fiji::controller::DeckController *m) {
		    QJsonArray running; // obs_data arrays only hold objects
		    for (const auto &button : m->running()) {
			    running.append(QJsonObject{{QStringLiteral("button"), button}});
		    }
		    put_array(response, "running", running);
	    })) {
		obs_data_set_bool(response, "success", true);
	}
}

void vendor_request_get_triggers(obs_data_t *, obs_data_t *response, void *)
{
	if (on_decks(response, [response](fiji::controller::DeckController *m) { put_array(response, "triggers", m->triggers()); })) {
		obs_data_set_bool(response, "success", true);
	}
}

// Every request, for OBS WebSocket clients and for the bot link alike.
static const struct {
	const char *name;
	void (*handler)(obs_data_t *request_data, obs_data_t *response_data, void *);
} requests[] = {
	{"version", vendor_request_version},
	{"get_look", vendor_request_get_look},
	{"get_link", vendor_request_get_link},
	{"get_canvas", vendor_request_get_canvas},
	{"switch_scene", vendor_request_switch_scene},
	{"current_scene", vendor_request_current_scene},
	{"get_scenes", vendor_request_get_scenes},
	{"get_outputs", vendor_request_get_outputs},
	{"start_output", vendor_request_start_output},
	{"stop_output", vendor_request_stop_output},
	{"start_all_outputs", vendor_request_start_all_outputs},
	{"stop_all_outputs", vendor_request_stop_all_outputs},
	{"start_all_streams", vendor_request_start_all_streams},
	{"stop_all_streams", vendor_request_stop_all_streams},
	{"start_all_recordings", vendor_request_start_all_recordings},
	{"stop_all_recordings", vendor_request_stop_all_recordings},
	{"save_backtrack", vendor_request_save_backtrack},
	{"add_chapter", vendor_request_add_chapter},
	{"get_docks", vendor_request_get_docks},
	{"get_studio_mode", vendor_request_get_studio_mode},
	{"set_studio_mode", vendor_request_set_studio_mode},
	{"set_preview_scene", vendor_request_set_preview_scene},
	{"transition", vendor_request_transition},
	{"show_multiview", vendor_request_show_multiview},
	{"get_groups", vendor_request_get_groups},
	{"start_group", vendor_request_start_group},
	{"stop_group", vendor_request_stop_group},
	{"dock_show", vendor_request_dock_show},
	{"dock_hide", vendor_request_dock_hide},
	{"get_live_scenes", vendor_request_get_live_scenes},
	{"live_scenes_add", vendor_request_live_scenes_add},
	{"live_scenes_remove", vendor_request_live_scenes_remove},
	{"canvas_dock_show_panel", vendor_request_dock_show_panel},
	{"canvas_dock_hide_panel", vendor_request_dock_hide_panel},
	{"get_transitions", vendor_request_get_transitions},
	{"switch_transition", vendor_request_switch_transition},
	{"transitions_add", vendor_request_transitions_add},
	{"transitions_remove", vendor_request_transitions_remove},
	{"get_status", vendor_request_get_status},
	{"set_main_scene", vendor_request_set_main_scene},
	{"start_main", vendor_request_start_main},
	{"stop_main", vendor_request_stop_main},
	{"get_decks", vendor_request_get_decks},
	{"save_deck", vendor_request_save_deck},
	{"delete_deck", vendor_request_delete_deck},
	{"press_button", vendor_request_press_button},
	{"release_button", vendor_request_release_button},
	{"stop_button", vendor_request_stop_button},
	{"get_deck_state", vendor_request_get_deck_state},
	{"get_triggers", vendor_request_get_triggers},
	{"fire_trigger", vendor_request_press_button},
	{"get_web_file", vendor_request_get_web_file},
};

bool fiji_call_request(const char *name, obs_data_t *request, obs_data_t *response)
{
	for (const auto &r : requests) {
		if (strcmp(r.name, name) == 0) {
			r.handler(request, response, nullptr);
			return true;
		}
	}
	return false;
}

void load_obs_websocket()
{
	vendor = obs_websocket_register_vendor("fiji-suite");
	for (const auto &r : requests) {
		obs_websocket_vendor_register_request(vendor, r.name, r.handler, nullptr);
	}
}

void unload_obs_websocket()
{
	if (!vendor) {
		return;
	}
	if (!obs_get_module("obs-websocket")) {
		vendor = nullptr;
		return;
	}
	for (const auto &r : requests) {
		obs_websocket_vendor_unregister_request(vendor, r.name);
	}
}
