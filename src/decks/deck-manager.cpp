#include "deck-manager.hpp"

#include "../docks/canvas-dock.hpp"
#include "../utils/fiji-events.hpp"
#include <obs-frontend-api.h>
#include <obs-module.h>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QThread>
#include <QUuid>
#include <util/platform.h>

extern std::list<CanvasDock *> canvas_docks;

static DeckManager *manager = nullptr;

// Sounds play on output channels of their own, so they are mixed like any other audio source.
// OBS uses the low channels for the scene and its global audio devices.
static const int firstSoundChannel = 40;
static const int lastSoundChannel = 59;

// ------------------------------------------------------------------ JSON <-> obs_data
static OBSDataAutoRelease to_obs(const QJsonObject &object)
{
	return obs_data_create_from_json(QJsonDocument(object).toJson(QJsonDocument::Compact).constData());
}

static QJsonObject to_json(obs_data_t *data)
{
	return data ? QJsonDocument::fromJson(QByteArray(obs_data_get_json(data))).object() : QJsonObject();
}

static QString short_id()
{
	return QUuid::createUuid().toString(QUuid::Id128).left(12);
}

static QString decks_path()
{
	char *path = obs_module_config_path("decks.json");
	const QString result = QString::fromUtf8(path);
	bfree(path);
	return result;
}

// ------------------------------------------------------------------ step values
// After variables are filled in, numbers often arrive as text ("/$randomX$/" -> "281").
static double number(const QJsonObject &step, const char *key, double fallback = 0.0)
{
	const auto value = step.value(QLatin1String(key));
	if (value.isDouble()) {
		return value.toDouble();
	}
	bool ok = false;
	const double parsed = value.toString().toDouble(&ok);
	return ok ? parsed : fallback;
}

static QString text(const QJsonObject &step, const char *key)
{
	return step.value(QLatin1String(key)).toString();
}

// true, false or "toggle", given what the thing is now.
static bool flag(const QJsonObject &step, const char *key, bool current)
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
static bool is_main(const QString &canvas)
{
	return canvas.isEmpty() || canvas.compare(QStringLiteral("main"), Qt::CaseInsensitive) == 0;
}

static CanvasDock *dock_of(const QString &canvas)
{
	for (const auto &dock : canvas_docks) {
		if (canvas.compare(QString::fromUtf8(obs_canvas_get_name(dock->GetCanvas())), Qt::CaseInsensitive) == 0) {
			return dock;
		}
	}
	return nullptr;
}

// A source by name: on the given canvas, or anywhere when the canvas is the main one.
static OBSSourceAutoRelease source_named(const QString &canvas, const QString &name)
{
	if (name.isEmpty()) {
		return nullptr;
	}
	if (!is_main(canvas)) {
		if (auto dock = dock_of(canvas)) {
			if (auto found = obs_canvas_get_source_by_name(dock->GetCanvas(), name.toUtf8().constData())) {
				return found;
			}
		}
	}
	return obs_get_source_by_name(name.toUtf8().constData());
}

// The scene a step means: by name, or the one the canvas is showing when no name is given.
static OBSSourceAutoRelease scene_of(const QString &canvas, const QString &scene)
{
	if (!scene.isEmpty()) {
		return source_named(canvas, scene);
	}
	if (is_main(canvas)) {
		return obs_frontend_get_current_scene();
	}
	if (auto dock = dock_of(canvas)) {
		return source_named(canvas, dock->GetProgramSceneName());
	}
	return nullptr;
}

static OBSSceneItem item_of(const QJsonObject &step)
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

static void warn(const QJsonObject &step, const char *problem)
{
	blog(LOG_WARNING, "[Fiji Suite] Deck step '%s': %s (%s)", text(step, "type").toUtf8().constData(), problem,
	     QJsonDocument(step).toJson(QJsonDocument::Compact).left(300).constData());
}

// ------------------------------------------------------------------ lifetime
void DeckManager::Create()
{
	if (!manager) {
		manager = new DeckManager;
	}
}

void DeckManager::Destroy()
{
	delete manager;
	manager = nullptr;
}

DeckManager *DeckManager::Get()
{
	return manager;
}

DeckManager::DeckManager() : engine(this)
{
	clock.start();
	ticker.setInterval(15);
	connect(&ticker, &QTimer::timeout, this, [this] {
		engine.tick(clock.elapsed());
		if (engine.idle()) {
			ticker.stop();
		}
	});
	Load();
	engine.setDecks(decks);
	RegisterHotkeys();
}

DeckManager::~DeckManager()
{
	Shutdown();
	UnregisterHotkeys(false);
}

void DeckManager::Shutdown()
{
	ticker.stop();
	engine.stopAll();
	StopSounds(QString());
}

void DeckManager::Wake()
{
	if (!ticker.isActive()) {
		ticker.start();
	}
}

// ------------------------------------------------------------------ storage
void DeckManager::Load()
{
	QFile file(decks_path());
	if (!file.open(QIODevice::ReadOnly)) {
		return; // no decks yet
	}
	QJsonParseError error;
	const auto document = QJsonDocument::fromJson(file.readAll(), &error);
	if (error.error != QJsonParseError::NoError || !document.isObject()) {
		blog(LOG_WARNING, "[Fiji Suite] decks.json could not be read (%s); it is left untouched",
		     error.errorString().toUtf8().constData());
		return;
	}
	const auto root = document.object();
	decks = root.value(QStringLiteral("decks")).toArray();
	soundMonitoring = root.value(QStringLiteral("sound_monitoring")).toBool(true);
}

void DeckManager::Save()
{
	const QString path = decks_path();
	QDir().mkpath(QFileInfo(path).absolutePath());
	// keep the previous file as a backup before replacing it
	if (QFile::exists(path)) {
		QFile::remove(path + QStringLiteral(".bak"));
		QFile::copy(path, path + QStringLiteral(".bak"));
	}
	QJsonObject root{{QStringLiteral("version"), 1},
			 {QStringLiteral("sound_monitoring"), soundMonitoring},
			 {QStringLiteral("decks"), decks}};
	QSaveFile file(path); // writes to a temporary file and swaps it in, so a crash cannot truncate decks.json
	if (!file.open(QIODevice::WriteOnly) || file.write(QJsonDocument(root).toJson()) < 0 || !file.commit()) {
		blog(LOG_WARNING, "[Fiji Suite] decks.json could not be saved");
	}
}

void DeckManager::Changed()
{
	UnregisterHotkeys(true);
	engine.setDecks(decks);
	RegisterHotkeys();
	Save();
	fiji_emit_event("decks_changed", nullptr);
}

QJsonObject DeckManager::SaveDeck(QJsonObject deck)
{
	// button ids are what presses, triggers and other buttons refer to: unique across all decks
	QSet<QString> taken;
	const QString deckId = deck.value(QStringLiteral("id")).toString();
	for (const auto &other : decks) {
		if (other.toObject().value(QStringLiteral("id")).toString() == deckId && !deckId.isEmpty()) {
			continue;
		}
		for (const auto &button : other.toObject().value(QStringLiteral("buttons")).toArray()) {
			taken.insert(button.toObject().value(QStringLiteral("id")).toString());
		}
	}
	if (deckId.isEmpty()) {
		deck.insert(QStringLiteral("id"), short_id());
	}
	QJsonArray buttons;
	for (const auto &value : deck.value(QStringLiteral("buttons")).toArray()) {
		auto button = value.toObject();
		QString id = button.value(QStringLiteral("id")).toString();
		if (id.isEmpty() || taken.contains(id)) {
			id = short_id();
			button.insert(QStringLiteral("id"), id);
		}
		taken.insert(id);
		buttons.append(button);
	}
	deck.insert(QStringLiteral("buttons"), buttons);

	bool replaced = false;
	for (int i = 0; i < decks.size(); i++) {
		if (decks.at(i).toObject().value(QStringLiteral("id")) == deck.value(QStringLiteral("id"))) {
			decks.replace(i, deck);
			replaced = true;
			break;
		}
	}
	if (!replaced) {
		decks.append(deck);
	}
	Changed();
	return deck;
}

bool DeckManager::DeleteDeck(const QString &id)
{
	for (int i = 0; i < decks.size(); i++) {
		if (decks.at(i).toObject().value(QStringLiteral("id")).toString() == id) {
			decks.removeAt(i);
			Changed();
			return true;
		}
	}
	return false;
}

QJsonArray DeckManager::Triggers() const
{
	QJsonArray all;
	for (const auto &deckValue : decks) {
		const auto deck = deckValue.toObject();
		for (const auto &buttonValue : deck.value(QStringLiteral("buttons")).toArray()) {
			const auto button = buttonValue.toObject();
			for (const auto &triggerValue : button.value(QStringLiteral("triggers")).toArray()) {
				auto trigger = triggerValue.toObject();
				trigger.insert(QStringLiteral("button"), button.value(QStringLiteral("id")));
				trigger.insert(QStringLiteral("deck"), deck.value(QStringLiteral("id")));
				all.append(trigger);
			}
		}
	}
	return all;
}

// ------------------------------------------------------------------ pressing
bool DeckManager::Press(const QString &button, const decks::Variables &trigger)
{
	const bool started = engine.press(button, trigger, clock.elapsed());
	if (started) {
		engine.tick(clock.elapsed()); // steps at time zero run right away
		if (!engine.idle()) {
			Wake();
		}
	}
	return started;
}

bool DeckManager::Release(const QString &button)
{
	const bool started = engine.release(button, clock.elapsed());
	if (started) {
		engine.tick(clock.elapsed());
		if (!engine.idle()) {
			Wake();
		}
	}
	return started;
}

void DeckManager::Stop(const QString &button)
{
	if (button == QStringLiteral("all")) {
		engine.stopAll();
		StopSounds(QString());
	} else {
		engine.stop(button);
	}
	engine.tick(clock.elapsed()); // reports the stopped buttons as idle
}

void DeckManager::buttonStateChanged(const QString &button, bool running)
{
	if (!fiji_events_wanted()) {
		return;
	}
	OBSDataAutoRelease data = obs_data_create();
	obs_data_set_string(data, "button", button.toUtf8().constData());
	obs_data_set_bool(data, "running", running);
	fiji_emit_event("button_state", data);
}

// ------------------------------------------------------------------ hotkeys
void DeckManager::RegisterHotkeys()
{
	for (const auto &deckValue : decks) {
		const auto deck = deckValue.toObject();
		for (const auto &buttonValue : deck.value(QStringLiteral("buttons")).toArray()) {
			const auto button = buttonValue.toObject();
			const QString id = button.value(QStringLiteral("id")).toString();
			const QString label = QString::fromUtf8(obs_module_text("DeckHotkey"))
						      .arg(deck.value(QStringLiteral("name")).toString(),
							   button.value(QStringLiteral("text")).toString().simplified());
			Hotkey hotkey;
			hotkey.button = id;
			hotkey.id = obs_hotkey_register_frontend(
				(QStringLiteral("FijiSuiteDeck") + id).toUtf8().constData(), label.toUtf8().constData(),
				[](void *, obs_hotkey_id id, obs_hotkey_t *, bool pressed) {
					// the manager outlives its hotkeys, and finds the button by the hotkey's id
					QMetaObject::invokeMethod(
						manager,
						[id, pressed] {
							for (const auto &h : manager->hotkeys) {
								if (h.id == id) {
									pressed ? manager->Press(h.button, {}) : manager->Release(h.button);
									return;
								}
							}
						},
						Qt::QueuedConnection);
				},
				nullptr);
			const auto bindings = button.value(QStringLiteral("hotkey")).toArray();
			if (!bindings.isEmpty()) {
				OBSDataAutoRelease wrapper = to_obs(QJsonObject{{QStringLiteral("keys"), bindings}});
				OBSDataArrayAutoRelease keys = obs_data_get_array(wrapper, "keys");
				obs_hotkey_load(hotkey.id, keys);
			}
			hotkeys.push_back(hotkey);
		}
	}
}

void DeckManager::UnregisterHotkeys(bool keepBindings)
{
	if (keepBindings) {
		// carry what the user bound in OBS's hotkey settings into the decks, so it is saved with them
		for (int d = 0; d < decks.size(); d++) {
			auto deck = decks.at(d).toObject();
			auto buttons = deck.value(QStringLiteral("buttons")).toArray();
			for (int b = 0; b < buttons.size(); b++) {
				auto button = buttons.at(b).toObject();
				for (const auto &h : hotkeys) {
					if (h.button != button.value(QStringLiteral("id")).toString()) {
						continue;
					}
					OBSDataArrayAutoRelease keys = obs_hotkey_save(h.id);
					OBSDataAutoRelease wrapper = obs_data_create();
					obs_data_set_array(wrapper, "keys", keys);
					button.insert(QStringLiteral("hotkey"), to_json(wrapper).value(QStringLiteral("keys")).toArray());
				}
				buttons.replace(b, button);
			}
			deck.insert(QStringLiteral("buttons"), buttons);
			decks.replace(d, deck);
		}
	}
	for (const auto &h : hotkeys) {
		obs_hotkey_unregister(h.id);
	}
	hotkeys.clear();
}

// ------------------------------------------------------------------ sounds
void DeckManager::SoundEnded(void *data, calldata_t *calldata)
{
	auto self = static_cast<DeckManager *>(data);
	OBSWeakSource ended = OBSGetWeakRef((obs_source_t *)calldata_ptr(calldata, "source"));
	// signalled from the media thread
	QMetaObject::invokeMethod(
		self,
		[self, ended] {
			OBSSource source = OBSGetStrongRef(ended);
			for (auto it = self->sounds.begin(); it != self->sounds.end(); ++it) {
				if (it->source == source) {
					signal_handler_disconnect(obs_source_get_signal_handler(it->source), "media_ended", SoundEnded, self);
					obs_set_output_source(it->channel, nullptr);
					self->sounds.erase(it);
					return;
				}
			}
		},
		Qt::QueuedConnection);
}

void DeckManager::PlaySound(const QJsonObject &step)
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
	obs_source_set_monitoring_type(source, soundMonitoring ? OBS_MONITORING_TYPE_MONITOR_AND_OUTPUT : OBS_MONITORING_TYPE_NONE);
	signal_handler_connect(obs_source_get_signal_handler(source), "media_ended", SoundEnded, this);
	obs_set_output_source(channel, source);
	sounds.push_back({channel, source.Get(), text(step, "name")});
}

void DeckManager::StopSounds(const QString &name)
{
	for (auto it = sounds.begin(); it != sounds.end();) {
		if (!name.isEmpty() && it->name != name) {
			++it;
			continue;
		}
		signal_handler_disconnect(obs_source_get_signal_handler(it->source), "media_ended", SoundEnded, this);
		obs_set_output_source(it->channel, nullptr);
		it = sounds.erase(it);
	}
}

// ------------------------------------------------------------------ steps
void DeckManager::execute(const QJsonObject &step)
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
		} else if (auto dock = dock_of(canvas)) {
			QMetaObject::invokeMethod(dock, "SwitchScene", Q_ARG(QString, scene));
		} else {
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
		PlaySound(step);
	} else if (type == QStringLiteral("stop_sounds")) {
		StopSounds(text(step, "name"));
	} else if (type == QStringLiteral("chat")) {
		// only the bot can talk in chat; with no bot connected this does nothing
		OBSDataAutoRelease data = obs_data_create();
		obs_data_set_string(data, "text", text(step, "text").toUtf8().constData());
		fiji_emit_event("say", data);
	} else if (type == QStringLiteral("notify")) {
		OBSDataAutoRelease data = obs_data_create();
		obs_data_set_string(data, "text", text(step, "text").toUtf8().constData());
		fiji_emit_event("deck_notification", data);
		blog(LOG_INFO, "[Fiji Suite] Deck: %s", text(step, "text").toUtf8().constData());
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

// ------------------------------------------------------------------ requests
// Requests arrive on the caller's thread; the manager lives on the UI thread.
static bool on_manager(obs_data_t *response, const std::function<void(DeckManager *)> &fn)
{
	if (!manager) {
		obs_data_set_string(response, "error", "Decks are not available");
		obs_data_set_bool(response, "success", false);
		return false;
	}
	if (QThread::currentThread() == manager->thread()) {
		fn(manager);
	} else {
		QMetaObject::invokeMethod(manager, [&fn] { fn(manager); }, Qt::BlockingQueuedConnection);
	}
	return true;
}

static void put_array(obs_data_t *response, const char *key, const QJsonArray &array)
{
	OBSDataAutoRelease wrapper = to_obs(QJsonObject{{QLatin1String(key), array}});
	OBSDataArrayAutoRelease value = obs_data_get_array(wrapper, key);
	obs_data_set_array(response, key, value);
}

void vendor_request_get_decks(obs_data_t *, obs_data_t *response, void *)
{
	if (on_manager(response, [response](DeckManager *m) { put_array(response, "decks", m->Decks()); })) {
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
	const QJsonObject wanted = to_json(deck);
	if (on_manager(response, [&](DeckManager *m) {
		    OBSDataAutoRelease saved = to_obs(m->SaveDeck(wanted));
		    obs_data_set_obj(response, "deck", saved);
	    })) {
		obs_data_set_bool(response, "success", true);
	}
}

void vendor_request_delete_deck(obs_data_t *request, obs_data_t *response, void *)
{
	const QString id = QString::fromUtf8(obs_data_get_string(request, "id"));
	bool found = false;
	if (!on_manager(response, [&](DeckManager *m) { found = m->DeleteDeck(id); })) {
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
	const QJsonObject values = to_json(data);
	for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
		trigger.insert(it.key(), it.value().toVariant());
	}
	bool started = false;
	if (!on_manager(response, [&](DeckManager *m) { started = m->Press(button, trigger); })) {
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
	if (on_manager(response, [&](DeckManager *m) { m->Release(button); })) {
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
	if (on_manager(response, [&](DeckManager *m) { m->Stop(button); })) {
		obs_data_set_bool(response, "success", true);
	}
}

void vendor_request_get_deck_state(obs_data_t *, obs_data_t *response, void *)
{
	if (on_manager(response, [response](DeckManager *m) {
		    QJsonArray running; // obs_data arrays only hold objects
		    for (const auto &button : m->Running()) {
			    running.append(QJsonObject{{QStringLiteral("button"), button}});
		    }
		    put_array(response, "running", running);
	    })) {
		obs_data_set_bool(response, "success", true);
	}
}

void vendor_request_get_triggers(obs_data_t *, obs_data_t *response, void *)
{
	if (on_manager(response, [response](DeckManager *m) { put_array(response, "triggers", m->Triggers()); })) {
		obs_data_set_bool(response, "success", true);
	}
}
