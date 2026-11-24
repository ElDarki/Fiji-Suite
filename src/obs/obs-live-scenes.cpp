#include "obs-live-scenes.hpp"

#include <obs.hpp>
#include <obs-frontend-api.h>

#include <QJsonDocument>
#include <QJsonObject>

namespace fiji::obs {

ObsLiveScenes::~ObsLiveScenes()
{
	if (!listening_) {
		return;
	}
	obs_frontend_remove_save_callback(saveLoad, this);
	if (obs_canvas_t *canvas = obs_get_main_canvas()) {
		signal_handler_disconnect(obs_canvas_get_signal_handler(canvas), "source_rename", sourceRenamed, this);
		obs_canvas_release(canvas);
	}
}

void ObsLiveScenes::listen(Listener listener)
{
	if (listening_) {
		return;
	}
	listener_ = std::move(listener);
	listening_ = true;
	obs_frontend_add_save_callback(saveLoad, this);
	if (obs_canvas_t *canvas = obs_get_main_canvas()) {
		signal_handler_connect(obs_canvas_get_signal_handler(canvas), "source_rename", sourceRenamed, this);
		obs_canvas_release(canvas);
	}
}

void ObsLiveScenes::saveLoad(obs_data_t *data, bool saving, void *param)
{
	auto self = static_cast<ObsLiveScenes *>(param);
	if (saving) {
		if (!self->listener_.save) {
			return;
		}
		// the list as JSON, set on the collection through a one-key wrapper
		const QJsonObject wrapper{{"live_scenes", self->listener_.save()}};
		OBSDataAutoRelease from = obs_data_create_from_json(QJsonDocument(wrapper).toJson(QJsonDocument::Compact).constData());
		OBSDataArrayAutoRelease scenes = obs_data_get_array(from, "live_scenes");
		obs_data_set_array(data, "live_scenes", scenes);
	} else if (self->listener_.load) {
		OBSDataArrayAutoRelease scenes = obs_data_get_array(data, "live_scenes");
		OBSDataAutoRelease wrapper = obs_data_create();
		if (scenes) {
			obs_data_set_array(wrapper, "live_scenes", scenes);
		}
		self->listener_.load(QJsonDocument::fromJson(QByteArray(obs_data_get_json(wrapper))).object().value("live_scenes").toArray());
	}
}

void ObsLiveScenes::sourceRenamed(void *param, calldata_t *cd)
{
	auto self = static_cast<ObsLiveScenes *>(param);
	if (self->listener_.renamed) {
		self->listener_.renamed(QString::fromUtf8(calldata_string(cd, "prev_name")),
					QString::fromUtf8(calldata_string(cd, "new_name")));
	}
}

QStringList ObsLiveScenes::sceneNames()
{
	QStringList out;
	char **names = obs_frontend_get_scene_names();
	for (char **name = names; name && *name; name++) {
		out.append(QString::fromUtf8(*name));
	}
	bfree(names);
	return out;
}

QString ObsLiveScenes::uuidOf(const QString &name)
{
	OBSSourceAutoRelease scene = obs_get_source_by_name(name.toUtf8().constData());
	return scene ? QString::fromUtf8(obs_source_get_uuid(scene)) : QString();
}

LiveScenesObs::Program ObsLiveScenes::program()
{
	OBSSourceAutoRelease scene = obs_frontend_get_current_scene();
	if (!scene) {
		return {};
	}
	return {QString::fromUtf8(obs_source_get_name(scene)), QString::fromUtf8(obs_source_get_uuid(scene))};
}

void ObsLiveScenes::showScene(const QString &uuid, const QString &name)
{
	OBSSourceAutoRelease scene = obs_get_source_by_uuid(uuid.toUtf8().constData());
	if (!scene) {
		scene = obs_get_source_by_name(name.toUtf8().constData());
	}
	if (!scene) {
		return;
	}
	OBSSourceAutoRelease current = obs_frontend_get_current_scene();
	if (scene.Get() != current.Get()) {
		obs_frontend_set_current_scene(scene);
	}
}

} // namespace fiji::obs
