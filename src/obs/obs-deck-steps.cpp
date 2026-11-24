#include "obs-deck-steps.hpp"

#include <obs-frontend-api.h>
#include <obs.hpp>
#include <util/platform.h>

#include <QFile>
#include <QJsonDocument>
#include <QMetaObject>
#include <QRegularExpression>
#include <vector>

// The plugin's request table (src/utils/obs-websocket.cpp): the "request" step, and switching a Fiji Suite canvas's
// scene the way its dock does (with its transition).
bool fiji_call_request(const char *name, obs_data_t *request, obs_data_t *response);

namespace fiji::obs {

namespace {

const int kFirstSoundChannel = 40;
const int kLastSoundChannel = 59;

// After variables are filled in, numbers often arrive as text ("/$randomX$/" -> "281").
double number(const QJsonObject &step, const char *key, double fallback = 0.0)
{
	const auto value = step.value(QLatin1String(key));
	if (value.isDouble()) {
		return value.toDouble();
	}
	bool ok = false;
	const double parsed = value.toString().toDouble(&ok);
	return ok ? parsed : fallback;
}

QString text(const QJsonObject &step, const char *key)
{
	return step.value(QLatin1String(key)).toString();
}

// true, false or "toggle", given what the thing is now.
bool flag(const QJsonObject &step, const char *key, bool current)
{
	const auto value = step.value(QLatin1String(key));
	if (value.isBool()) {
		return value.toBool();
	}
	const auto word = value.toString().toLower();
	if (word == QStringLiteral("toggle")) {
		return !current;
	}
	return value.isDouble() ? value.toDouble() != 0 : (word == QStringLiteral("true") || word == QStringLiteral("1"));
}

bool warn(const QJsonObject &step, const char *problem)
{
	blog(LOG_WARNING, "[Fiji Suite] Deck step '%s': %s (%s)", text(step, "type").toUtf8().constData(), problem,
	     QJsonDocument(step).toJson(QJsonDocument::Compact).left(300).constData());
	return false;
}

bool is_main(const QString &canvas)
{
	return canvas.isEmpty() || canvas.compare(QStringLiteral("main"), Qt::CaseInsensitive) == 0;
}

// A Fiji Suite canvas by name, ignoring case. Returns a new reference.
OBSCanvasAutoRelease canvas_named(const QString &name)
{
	struct Find {
		QString name;
		obs_canvas_t *found = nullptr;
	} find{name};
	obs_enum_canvases(
		[](void *param, obs_canvas_t *canvas) {
			auto f = static_cast<Find *>(param);
			if (f->name.compare(QString::fromUtf8(obs_canvas_get_name(canvas)), Qt::CaseInsensitive) == 0) {
				f->found = obs_canvas_get_ref(canvas);
				return false;
			}
			return true;
		},
		&find);
	return find.found;
}

// A source by name: on the given canvas, or anywhere when the canvas is the main one.
OBSSourceAutoRelease source_named(const QString &canvas, const QString &name)
{
	if (name.isEmpty()) {
		return nullptr;
	}
	if (!is_main(canvas)) {
		if (OBSCanvasAutoRelease c = canvas_named(canvas)) {
			if (auto found = obs_canvas_get_source_by_name(c, name.toUtf8().constData())) {
				return found;
			}
		}
	}
	return obs_get_source_by_name(name.toUtf8().constData());
}

// The scene a step means: by name, or the one the canvas has on program when no name is given.
OBSSourceAutoRelease scene_of(const QString &canvas, const QString &scene)
{
	if (!scene.isEmpty()) {
		return source_named(canvas, scene);
	}
	if (is_main(canvas)) {
		return obs_frontend_get_current_scene();
	}
	OBSCanvasAutoRelease c = canvas_named(canvas);
	if (!c) {
		return nullptr;
	}
	OBSSourceAutoRelease program = obs_canvas_get_channel(c, 0);
	if (program && obs_source_get_type(program) == OBS_SOURCE_TYPE_TRANSITION) {
		return obs_transition_get_active_source(program); // a canvas shows its scene through its transition
	}
	return program.Get() ? obs_source_get_ref(program) : nullptr;
}

OBSSceneItem item_of(const QJsonObject &step)
{
	OBSSourceAutoRelease sceneSource = scene_of(text(step, "canvas"), text(step, "scene"));
	obs_scene_t *scene = obs_group_or_scene_from_source(sceneSource);
	if (!scene) {
		return nullptr;
	}
	// "# 21" means the scene's item number 21, which is how SAMMI decks can name a source
	static const QRegularExpression byNumber(QStringLiteral("^#\\s*(\\d+)$"));
	const auto numbered = byNumber.match(text(step, "source").trimmed());
	if (numbered.hasMatch()) {
		return obs_scene_find_sceneitem_by_id(scene, numbered.captured(1).toLongLong());
	}
	return obs_scene_find_source_recursive(scene, text(step, "source").toUtf8().constData());
}

OBSDataAutoRelease to_obs(const QJsonObject &object)
{
	return obs_data_create_from_json(QJsonDocument(object).toJson(QJsonDocument::Compact).constData());
}

} // namespace

// ---------------------------------------------------------------------- sounds
struct ObsDeckSteps::Sounds {
	struct Sound {
		int channel;
		OBSSource source;
		QString name;
	};
	std::vector<Sound> playing;

	static void ended(void *data, calldata_t *calldata);
	void remove(std::vector<Sound>::iterator it, ObsDeckSteps *owner)
	{
		signal_handler_disconnect(obs_source_get_signal_handler(it->source), "media_ended", ended, owner);
		obs_set_output_source(it->channel, nullptr);
		playing.erase(it);
	}
};

void ObsDeckSteps::Sounds::ended(void *data, calldata_t *calldata)
{
	auto owner = static_cast<ObsDeckSteps *>(data);
	OBSWeakSource done = OBSGetWeakRef((obs_source_t *)calldata_ptr(calldata, "source"));
	// signalled from the media thread
	QMetaObject::invokeMethod(
		owner,
		[owner, done] {
			OBSSource source = OBSGetStrongRef(done);
			auto &playing = owner->sounds_->playing;
			for (auto it = playing.begin(); it != playing.end(); ++it) {
				if (it->source == source) {
					owner->sounds_->remove(it, owner);
					return;
				}
			}
		},
		Qt::QueuedConnection);
}

ObsDeckSteps::ObsDeckSteps(QObject *parent) : QObject(parent), sounds_(std::make_unique<Sounds>()) {}

ObsDeckSteps::~ObsDeckSteps()
{
	stopSounds(QString());
}

bool ObsDeckSteps::playSound(const QJsonObject &step)
{
	const QString file = text(step, "file");
	if (!QFile::exists(file)) {
		return warn(step, "sound file not found");
	}
	int channel = -1;
	for (int c = kFirstSoundChannel; c <= kLastSoundChannel; c++) {
		OBSSourceAutoRelease taken = obs_get_output_source(c);
		if (!taken) {
			channel = c;
			break;
		}
	}
	if (channel < 0) {
		return warn(step, "too many sounds are playing");
	}
	OBSDataAutoRelease settings = obs_data_create();
	obs_data_set_string(settings, "local_file", file.toUtf8().constData());
	obs_data_set_bool(settings, "is_local_file", true);
	obs_data_set_bool(settings, "looping", false);
	obs_data_set_bool(settings, "close_when_inactive", false);
	OBSSourceAutoRelease source = obs_source_create_private("ffmpeg_source", "Fiji Suite deck sound", settings);
	if (!source) {
		return warn(step, "the media source could not be created");
	}
	obs_source_set_volume(source, float(number(step, "volume", 1.0)));
#if LIBOBS_API_MAJOR_VER >= 33
	obs_source_set_monitoring_enabled(source, monitoring_); // OBS 33 replaced the monitoring types with on/off
#else
	obs_source_set_monitoring_type(source, monitoring_ ? OBS_MONITORING_TYPE_MONITOR_AND_OUTPUT : OBS_MONITORING_TYPE_NONE);
#endif
	signal_handler_connect(obs_source_get_signal_handler(source), "media_ended", Sounds::ended, this);
	obs_set_output_source(channel, source);
	sounds_->playing.push_back({channel, source.Get(), text(step, "name")});
	return true;
}

void ObsDeckSteps::stopSounds(const QString &name)
{
	auto &playing = sounds_->playing;
	for (auto it = playing.begin(); it != playing.end();) {
		if (!name.isEmpty() && it->name != name) {
			++it;
			continue;
		}
		signal_handler_disconnect(obs_source_get_signal_handler(it->source), "media_ended", Sounds::ended, this);
		obs_set_output_source(it->channel, nullptr);
		it = playing.erase(it);
	}
}

// ---------------------------------------------------------------------- steps
bool ObsDeckSteps::run(const QJsonObject &step)
{
	const QString type = text(step, "type");
	const QString canvas = text(step, "canvas");

	if (type == QStringLiteral("scene")) {
		const QString scene = text(step, "scene");
		if (is_main(canvas)) {
			OBSSourceAutoRelease source = obs_get_source_by_name(scene.toUtf8().constData());
			if (!source || !obs_source_is_scene(source)) {
				return warn(step, "scene not found");
			}
			obs_frontend_set_current_scene(source);
			return true;
		}
		OBSDataAutoRelease request = to_obs(QJsonObject{{QStringLiteral("canvas"), canvas}, {QStringLiteral("scene"), scene}});
		OBSDataAutoRelease response = obs_data_create();
		if (!fiji_call_request("switch_scene", request, response) || !obs_data_get_bool(response, "success")) {
			return warn(step, "canvas not found");
		}
		return true;
	}
	if (type == QStringLiteral("source_visibility")) {
		OBSSceneItem item = item_of(step);
		if (!item) {
			return warn(step, "source not found in that scene");
		}
		obs_sceneitem_set_visible(item, flag(step, "visible", obs_sceneitem_visible(item)));
		return true;
	}
	if (type == QStringLiteral("source_position") || type == QStringLiteral("source_scale") ||
	    type == QStringLiteral("source_rotation")) {
		OBSSceneItem item = item_of(step);
		if (!item) {
			return warn(step, "source not found in that scene");
		}
		if (type == QStringLiteral("source_rotation")) {
			obs_sceneitem_set_rot(item, float(number(step, "rotation")));
		} else {
			vec2 value;
			vec2_set(&value, float(number(step, "x")), float(number(step, "y")));
			type == QStringLiteral("source_position") ? obs_sceneitem_set_pos(item, &value)
								  : obs_sceneitem_set_scale(item, &value);
		}
		return true;
	}
	if (type == QStringLiteral("filter_visibility")) {
		// no source named: the scene the canvas is showing (scenes can carry filters too)
		OBSSourceAutoRelease source = text(step, "source").isEmpty() ? scene_of(canvas, QString())
									      : source_named(canvas, text(step, "source"));
		OBSSourceAutoRelease filter = source ? obs_source_get_filter_by_name(source, text(step, "filter").toUtf8().constData())
						     : nullptr;
		if (!filter) {
			return warn(step, "filter not found");
		}
		obs_source_set_enabled(filter, flag(step, "visible", obs_source_enabled(filter)));
		return true;
	}
	if (type == QStringLiteral("source_text")) {
		OBSSourceAutoRelease source = source_named(canvas, text(step, "source"));
		if (!source) {
			return warn(step, "source not found");
		}
		OBSDataAutoRelease settings = obs_data_create();
		obs_data_set_string(settings, "text", text(step, "text").toUtf8().constData());
		obs_source_update(source, settings);
		return true;
	}
	if (type == QStringLiteral("mute")) {
		OBSSourceAutoRelease source = source_named(canvas, text(step, "source"));
		if (!source) {
			return warn(step, "source not found");
		}
		obs_source_set_muted(source, flag(step, "muted", obs_source_muted(source)));
		return true;
	}
	if (type == QStringLiteral("sound")) {
		return playSound(step);
	}
	if (type == QStringLiteral("stop_sounds")) {
		stopSounds(text(step, "name"));
		return true;
	}
	if (type == QStringLiteral("request")) {
		// any plugin request: outputs, groups, studio mode, ... (see docs/websocket.md)
		OBSDataAutoRelease request = to_obs(step.value(QStringLiteral("data")).toObject());
		OBSDataAutoRelease response = obs_data_create();
		if (!fiji_call_request(text(step, "request").toUtf8().constData(), request, response) ||
		    !obs_data_get_bool(response, "success")) {
			return warn(step, obs_data_get_string(response, "error")[0] ? obs_data_get_string(response, "error")
										  : "request failed");
		}
		return true;
	}
	return warn(step, "unknown step type");
}

} // namespace fiji::obs
