#include "obs-live-scenes.hpp"

#include "../model/live-scenes.hpp"

#include <obs-frontend-api.h>
#include <obs.hpp>

#include <QMetaObject>

namespace fiji::obs {

// ---- ObsScenes ------------------------------------------------------------------------------------------------
QStringList ObsScenes::sceneNames()
{
	QStringList names;
	char **list = obs_frontend_get_scene_names();
	for (char **name = list; name && *name; name++) {
		names.append(QString::fromUtf8(*name));
	}
	bfree(list);
	return names;
}

QString ObsScenes::uuidOf(const QString &name)
{
	OBSSourceAutoRelease scene = obs_get_source_by_name(name.toUtf8().constData());
	return scene ? QString::fromUtf8(obs_source_get_uuid(scene)) : QString();
}

SceneRef ObsScenes::programScene()
{
	OBSSourceAutoRelease scene = obs_frontend_get_current_scene();
	if (!scene) {
		return {};
	}
	return {QString::fromUtf8(obs_source_get_name(scene)), QString::fromUtf8(obs_source_get_uuid(scene))};
}

bool ObsScenes::showOnProgram(const SceneRef &ref)
{
	OBSSourceAutoRelease scene = ref.uuid.isEmpty() ? nullptr : obs_get_source_by_uuid(ref.uuid.toUtf8().constData());
	if (!scene) {
		scene = obs_get_source_by_name(ref.name.toUtf8().constData());
	}
	if (!scene) {
		return false;
	}
	OBSSourceAutoRelease current = obs_frontend_get_current_scene();
	if (scene.Get() != current.Get()) {
		obs_frontend_set_current_scene(scene);
	}
	return true;
}

// ---- ObsLiveScenesSync ----------------------------------------------------------------------------------------
ObsLiveScenesSync::ObsLiveScenesSync(model::LiveScenes &scenes) : scenes_(scenes)
{
	obs_frontend_add_save_callback(save_load, this);
}

ObsLiveScenesSync::~ObsLiveScenesSync()
{
	obs_frontend_remove_save_callback(save_load, this);
	followRenames(false);
}

void ObsLiveScenesSync::followRenames(bool follow)
{
	OBSCanvasAutoRelease main_canvas = obs_get_main_canvas();
	if (!main_canvas) {
		return;
	}
	auto sh = obs_canvas_get_signal_handler(main_canvas);
	signal_handler_disconnect(sh, "source_rename", source_rename, this);
	if (follow) {
		signal_handler_connect(sh, "source_rename", source_rename, this);
	}
}

void ObsLiveScenesSync::save_load(obs_data_t *save_data, bool saving, void *data)
{
	auto self = static_cast<ObsLiveScenesSync *>(data);
	if (saving) {
		OBSDataArrayAutoRelease array = obs_data_array_create();
		for (const auto &entry : self->scenes_.entries()) {
			OBSDataAutoRelease item = obs_data_create();
			obs_data_set_string(item, "name", entry.name.toUtf8().constData());
			obs_data_set_string(item, "uuid", entry.uuid.toUtf8().constData());
			obs_data_array_push_back(array, item);
		}
		obs_data_set_array(save_data, "live_scenes", array);
		return;
	}
	QList<model::LiveScene> entries;
	OBSDataArrayAutoRelease array = obs_data_get_array(save_data, "live_scenes");
	const size_t count = array ? obs_data_array_count(array) : 0;
	for (size_t i = 0; i < count; i++) {
		OBSDataAutoRelease item = obs_data_array_item(array, i);
		if (item) {
			entries.append({QString::fromUtf8(obs_data_get_string(item, "name")),
					QString::fromUtf8(obs_data_get_string(item, "uuid"))});
		}
	}
	self->scenes_.replaceAll(entries);
	self->followRenames(true);
}

// On OBS's thread: the model changes on its own.
void ObsLiveScenesSync::source_rename(void *data, calldata_t *cd)
{
	auto self = static_cast<ObsLiveScenesSync *>(data);
	const QString name = QString::fromUtf8(calldata_string(cd, "new_name"));
	const QString previous = QString::fromUtf8(calldata_string(cd, "prev_name"));
	auto &scenes = self->scenes_;
	QMetaObject::invokeMethod(&scenes, [&scenes, previous, name] { scenes.renamed(previous, name); },
				  Qt::QueuedConnection);
}

} // namespace fiji::obs
