#include "obs-deck-steps.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <obs.hpp>

#include <QFile>
#include <QJsonDocument>
#include <QMetaObject>
#include <QRegularExpression>

// The plugin's own canvases (src/docks/canvas-dock.cpp) and request table (src/utils/obs-websocket.cpp).
obs_canvas_t *fiji_find_canvas(const char *name); // borrowed; null when no Fiji Suite canvas has that name
QString fiji_canvas_program_scene(const char *canvas);
bool fiji_canvas_switch_scene(const char *canvas, const char *scene);
bool fiji_call_request(const char *name, obs_data_t *request, obs_data_t *response);

namespace fiji::obs {

namespace {

// Sounds play on output channels of their own, so they are mixed like any other audio source.
// OBS uses the low channels for the scene and its global audio devices.
const int firstSoundChannel = 40;
const int lastSoundChannel = 59;

OBSDataAutoRelease to_obs(const QJsonObject &object)
{
	return obs_data_create_from_json(QJsonDocument(object).toJson(QJsonDocument::Compact).constData());
}

// ------------------------------------------------------------------ step values
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

// ------------------------------------------------------------------ finding things in OBS
bool is_main(const QString &canvas)
{
	return canvas.isEmpty() || canvas.compare(QStringLiteral("main"), Qt::CaseInsensitive) == 0;
}


// A source by name: on the given canvas, or anywhere when the canvas is the main one.
OBSSourceAutoRelease source_named(const QString &canvas, const QString &name)
{
	if (name.isEmpty()) {
		return nullptr;
	}
	if (!is_main(canvas)) {
		if (auto found_canvas = fiji_find_canvas(canvas.toUtf8().constData())) {
			if (auto found = obs_canvas_get_source_by_name(found_canvas, name.toUtf8().constData())) {
				return found;
			}
		}
	}
	return obs_get_source_by_name(name.toUtf8().constData());
}

// The scene a step means: by name, or the one the canvas is showing when no name is given.
OBSSourceAutoRelease scene_of(const QString &canvas, const QString &scene)
{
	if (!scene.isEmpty()) {
		return source_named(canvas, scene);
	}
	if (is_main(canvas)) {
		return obs_frontend_get_current_scene();
	}
	if (fiji_find_canvas(canvas.toUtf8().constData())) {
		return source_named(canvas, fiji_canvas_program_scene(canvas.toUtf8().constData()));
	}
	return nullptr;
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

void warn(const QJsonObject &step, const char *problem)
{
	blog(LOG_WARNING, "[Fiji Suite] Deck step '%s': %s (%s)", text(step, "type").toUtf8().constData(), problem,
	     QJsonDocument(step).toJson(QJsonDocument::Compact).left(300).constData());
}


} // namespace

ObsDeckSteps::~ObsDeckSteps()
{
	stopSounds(QString());
}

void ObsDeckSteps::setSoundMonitoring(bool on)
{
	soundMonitoring = on;
}

// ------------------------------------------------------------------ sounds
void ObsDeckSteps::soundEnded(void *data, calldata_t *calldata)
{
	auto self = static_cast<ObsDeckSteps *>(data);
	OBSWeakSource ended = OBSGetWeakRef((obs_source_t *)calldata_ptr(calldata, "source"));
	// signalled from the media thread
	QMetaObject::invokeMethod(
		self,
		[self, ended] {
			OBSSource source = OBSGetStrongRef(ended);
			for (auto it = self->sounds.begin(); it != self->sounds.end(); ++it) {
				if (it->source == source) {
					signal_handler_disconnect(obs_source_get_signal_handler(it->source), "media_ended", soundEnded, self);
					obs_set_output_source(it->channel, nullptr);
					self->sounds.erase(it);
					return;
				}
			}
		},
		Qt::QueuedConnection);
}

void ObsDeckSteps::playSound(const QJsonObject &step)
{
	const QString file = text(step, "file");
	if (!QFile::exists(file)) {
		warn(step, "sound file not found");
		return;
	}
	int channel = -1;
	for (int c = firstSoundChannel; c <= lastSoundChannel; c++) {
		OBSSourceAutoRelease taken = obs_get_output_source(c);
		if (!taken) {
			channel = c;
			break;
		}
	}
	if (channel < 0) {
		warn(step, "too many sounds are playing");
		return;
	}
	OBSDataAutoRelease settings = obs_data_create();
	obs_data_set_string(settings, "local_file", file.toUtf8().constData());
	obs_data_set_bool(settings, "is_local_file", true);
	obs_data_set_bool(settings, "looping", false);
	obs_data_set_bool(settings, "close_when_inactive", false);
	OBSSourceAutoRelease source = obs_source_create_private("ffmpeg_source", "Fiji Suite deck sound", settings);
	if (!source) {
		warn(step, "the media source could not be created");
		return;
	}
	obs_source_set_volume(source, float(number(step, "volume", 1.0)));
#if LIBOBS_API_MAJOR_VER >= 33
	obs_source_set_monitoring_enabled(source, soundMonitoring); // OBS 33+: sources always play to the output
#else
	obs_source_set_monitoring_type(source, soundMonitoring ? OBS_MONITORING_TYPE_MONITOR_AND_OUTPUT : OBS_MONITORING_TYPE_NONE);
#endif
	signal_handler_connect(obs_source_get_signal_handler(source), "media_ended", soundEnded, this);
	obs_set_output_source(channel, source);
	sounds.push_back({channel, source.Get(), text(step, "name")});
}

void ObsDeckSteps::stopSounds(const QString &name)
{
	for (auto it = sounds.begin(); it != sounds.end();) {
		if (!name.isEmpty() && it->name != name) {
			++it;
			continue;
		}
		signal_handler_disconnect(obs_source_get_signal_handler(it->source), "media_ended", soundEnded, this);
		obs_set_output_source(it->channel, nullptr);
		it = sounds.erase(it);
	}
}

// ------------------------------------------------------------------ steps
void ObsDeckSteps::run(const QJsonObject &step)
{
	const QString type = text(step, "type");
	const QString canvas = text(step, "canvas");

	if (type == QStringLiteral("scene")) {
		const QString scene = text(step, "scene");
		if (is_main(canvas)) {
			OBSSourceAutoRelease source = obs_get_source_by_name(scene.toUtf8().constData());
			if (source && obs_source_is_scene(source)) {
				obs_frontend_set_current_scene(source);
			} else {
				warn(step, "scene not found");
			}
		} else if (!fiji_canvas_switch_scene(canvas.toUtf8().constData(), scene.toUtf8().constData())) {
			warn(step, "canvas not found");
		}
	} else if (type == QStringLiteral("source_visibility")) {
		OBSSceneItem item = item_of(step);
		if (item) {
			obs_sceneitem_set_visible(item, flag(step, "visible", obs_sceneitem_visible(item)));
		} else {
			warn(step, "source not found in that scene");
		}
	} else if (type == QStringLiteral("source_position") || type == QStringLiteral("source_scale") ||
		   type == QStringLiteral("source_rotation")) {
		OBSSceneItem item = item_of(step);
		if (!item) {
			warn(step, "source not found in that scene");
		} else if (type == QStringLiteral("source_rotation")) {
			obs_sceneitem_set_rot(item, float(number(step, "rotation")));
		} else {
			vec2 value;
			vec2_set(&value, float(number(step, "x")), float(number(step, "y")));
			type == QStringLiteral("source_position") ? obs_sceneitem_set_pos(item, &value)
								  : obs_sceneitem_set_scale(item, &value);
		}
	} else if (type == QStringLiteral("filter_visibility")) {
		// no source named: the scene the canvas is showing (scenes can carry filters too)
		OBSSourceAutoRelease source = text(step, "source").isEmpty() ? scene_of(canvas, QString())
									      : source_named(canvas, text(step, "source"));
		OBSSourceAutoRelease filter = source ? obs_source_get_filter_by_name(source, text(step, "filter").toUtf8().constData())
						     : nullptr;
		if (filter) {
			obs_source_set_enabled(filter, flag(step, "visible", obs_source_enabled(filter)));
		} else {
			warn(step, "filter not found");
		}
	} else if (type == QStringLiteral("source_text")) {
		OBSSourceAutoRelease source = source_named(canvas, text(step, "source"));
		if (source) {
			OBSDataAutoRelease settings = obs_data_create();
			obs_data_set_string(settings, "text", text(step, "text").toUtf8().constData());
			obs_source_update(source, settings);
		} else {
			warn(step, "source not found");
		}
	} else if (type == QStringLiteral("mute")) {
		OBSSourceAutoRelease source = source_named(canvas, text(step, "source"));
		if (source) {
			obs_source_set_muted(source, flag(step, "muted", obs_source_muted(source)));
		} else {
			warn(step, "source not found");
		}
	} else if (type == QStringLiteral("sound")) {
		playSound(step);
	} else if (type == QStringLiteral("stop_sounds")) {
		stopSounds(text(step, "name"));
	} else if (type == QStringLiteral("request")) {
		// any plugin request: outputs, groups, studio mode, ... (see docs/websocket.md)
		OBSDataAutoRelease request = to_obs(step.value(QStringLiteral("data")).toObject());
		OBSDataAutoRelease response = obs_data_create();
		if (!fiji_call_request(text(step, "request").toUtf8().constData(), request, response) ||
		    !obs_data_get_bool(response, "success")) {
			warn(step, obs_data_get_string(response, "error")[0] ? obs_data_get_string(response, "error") : "request failed");
		}
	} else {
		warn(step, "unknown step type");
	}
}

} // namespace fiji::obs
