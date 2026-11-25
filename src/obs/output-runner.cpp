#include "output-runner.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <obs.hpp>
#include <util/config-file.h>
#include <util/platform.h>

#include <QMetaObject>
#include <QPointer>
#include <QRegularExpression>

#include <string>
#include <vector>

// The stream dialog's check for Twitch servers (src/dialogs/stream-output-dialog.cpp).
bool isTwitchServer(QString outputServer);

namespace fiji::obs {

namespace {

bool encoder_available(const char *encoder)
{
	const char *val;
	int i = 0;

	while (obs_enum_encoder_types(i++, &val)) {
		if (strcmp(val, encoder) == 0) {
			return true;
		}
	}

	return false;
}

void ensure_directory(char *path)
{
#ifdef _WIN32
	char *backslash = strrchr(path, '\\');
	if (backslash) {
		*backslash = '/';
	}
#endif

	char *slash = strrchr(path, '/');
	if (slash) {
		*slash = 0;
		os_mkdirs(path);
		*slash = '/';
	}

#ifdef _WIN32
	if (backslash) {
		*backslash = '\\';
	}
#endif
}

} // namespace

OutputRunner::OutputRunner(QObject *parent) : QObject(parent) {}

OutputRunner::~OutputRunner()
{
	release();
}

obs_output *OutputRunner::output() const
{
	return output_;
}

bool OutputRunner::active() const
{
	return output_ && obs_output_active(output_);
}

void OutputRunner::setOnStarted(std::function<void()> onStarted)
{
	onStarted_ = std::move(onStarted);
}

void OutputRunner::stop()
{
	if (!active()) {
		return;
	}
	if (obs_output_get_active_delay(output_) > 0) {
		obs_output_stop(output_);
	} else {
		obs_output_force_stop(output_);
	}
}

void OutputRunner::release()
{
	if (!output_) {
		return;
	}
	signal_handler_t *signal = obs_output_get_signal_handler(output_);
	signal_handler_disconnect(signal, "start", output_start, this);
	signal_handler_disconnect(signal, "stop", output_stop, this);
	signal_handler_disconnect(signal, "reconnect", output_reconnect, this);
	signal_handler_disconnect(signal, "reconnect_success", output_reconnect_success, this);
	signal_handler_disconnect(signal, "saved", replay_saved, this);
	if (strcmp(obs_output_get_id(output_), "virtualcam_output") == 0) {
		obs_output_set_media(output_, obs_get_video(), obs_get_audio());
	}
	obs_output_release(output_);
	output_ = nullptr;
}

// ---- OBS's signals (on OBS's threads) ------------------------------------------------------------------------
void OutputRunner::output_start(void *data, calldata_t *)
{
	auto self = static_cast<OutputRunner *>(data);
	if (self->onStarted_) {
		self->onStarted_();
		self->onStarted_ = nullptr;
	}
	QMetaObject::invokeMethod(self, [self] { emit self->started(); }, Qt::QueuedConnection);
}

void OutputRunner::output_stop(void *data, calldata_t *calldata)
{
	auto self = static_cast<OutputRunner *>(data);
	const char *error = (const char *)calldata_ptr(calldata, "last_error");
	if (self->onStarted_) {
		if (error) {
			blog(LOG_WARNING, "[Fiji Suite] failed to start '%s': %s", self->name_.toUtf8().constData(), error);
		} else {
			blog(LOG_WARNING, "[Fiji Suite] failed to start '%s'", self->name_.toUtf8().constData());
		}
		self->onStarted_();
		self->onStarted_ = nullptr;
	}
	const QString lastError = error ? QString::fromUtf8(error) : QString();
	const long long code = calldata_int(calldata, "code");
	QMetaObject::invokeMethod(
		self,
		[self, lastError, code] {
			const bool hadOutput = self->output_ != nullptr;
			if (hadOutput) {
				if (strcmp(obs_output_get_id(self->output_), "virtualcam_output") == 0) {
					obs_output_set_media(self->output_, obs_get_video(), obs_get_audio());
				}
				obs_output_release(self->output_);
				self->output_ = nullptr;
			}
			emit self->stopped(lastError, code, hadOutput);
		},
		Qt::QueuedConnection);
}

void OutputRunner::output_reconnect(void *data, calldata_t *)
{
	auto self = static_cast<OutputRunner *>(data);
	QMetaObject::invokeMethod(self, [self] { emit self->reconnecting(); }, Qt::QueuedConnection);
}

void OutputRunner::output_reconnect_success(void *data, calldata_t *)
{
	auto self = static_cast<OutputRunner *>(data);
	QMetaObject::invokeMethod(self, [self] { emit self->reconnected(); }, Qt::QueuedConnection);
}

void OutputRunner::replay_saved(void *data, calldata_t *)
{
	auto self = static_cast<OutputRunner *>(data);
	QMetaObject::invokeMethod(self, [self] { emit self->replaySaved(); }, Qt::QueuedConnection);
}

// ---- starting ---------------------------------------------------------------------------------------------
bool OutputRunner::start(obs_data_t *settings)
{
	problem_.clear();
	virtualCamCanvas_.clear();
	if (!settings) {
		return false;
	}
	auto output_type = obs_data_get_string(settings, "type");
	isBacktrack_ = strcmp(output_type, "backtrack") == 0;
	const char *name = obs_data_get_string(settings, "name");
	name_ = QString::fromUtf8(name);
	if (output_) {
		auto service = obs_output_get_service(output_);
		if (obs_output_active(output_)) {
			obs_output_force_stop(output_);
		}
		obs_output_release(output_);
		obs_service_release(service);
		output_ = nullptr;
	}

	if (strcmp(output_type, "virtual_cam") == 0) {
		const auto vco = obs_frontend_get_virtualcam_output();
		if (obs_output_active(vco)) {
			problem_ = QStringLiteral("VirtualCamBusy");
			obs_output_release(vco);
			return false;
		}

		const char *canvas_name = obs_data_get_string(settings, "canvas");
		auto canvas = (!canvas_name || canvas_name[0] == '\0') ? obs_get_main_canvas()
								       : obs_get_canvas_by_name(canvas_name);
		if (!canvas) {
			return false;
		}

		signal_handler_t *signal = obs_output_get_signal_handler(vco);
		signal_handler_disconnect(signal, "start", output_start, this);
		signal_handler_disconnect(signal, "stop", output_stop, this);
		signal_handler_connect(signal, "start", output_start, this);
		signal_handler_connect(signal, "stop", output_stop, this);
		signal_handler_connect(signal, "reconnect", output_reconnect, this);
		signal_handler_connect(signal, "reconnect_success", output_reconnect_success, this);

		obs_output_set_media(vco, obs_canvas_get_video(canvas), obs_get_audio());
		if (!obs_output_start(vco)) {
			obs_canvas_release(canvas);
			return false;
		}
		output_ = vco;
		virtualCamCanvas_ = QString::fromUtf8(obs_canvas_get_name(canvas));
		obs_canvas_release(canvas);
		return true;
	} else if (strcmp(output_type, "ffmpeg") == 0) {

		const char *canvas_name = obs_data_get_string(settings, "canvas");
		auto canvas = (!canvas_name || canvas_name[0] == '\0') ? obs_get_main_canvas()
								       : obs_get_canvas_by_name(canvas_name);
		if (!canvas) {
			return false;
		}

		std::string output_name = "Fiji Suite Output ";
		output_name += name;
		output_ = obs_output_create("ffmpeg_output", output_name.c_str(), nullptr, nullptr);
		if (!output_) {
			blog(LOG_WARNING, "[Fiji Suite] failed to create ffmpeg output '%s'", name);
			return false;
		}

		OBSDataAutoRelease s = obs_data_create();
		OBSDataArrayAutoRelease audio_names = obs_data_array_create();
		for (size_t i = 0; i < MAX_AUDIO_MIXES; i++) {
			std::string cfg_name = "Track";
			cfg_name += std::to_string((int)i + 1);
			cfg_name += "Name";

			const char *audioName = config_get_string(obs_frontend_get_profile_config(), "AdvOut", cfg_name.c_str());
			OBSDataAutoRelease item = obs_data_create();
			if (audioName && audioName[0] != '\0') {
				obs_data_set_string(item, "name", audioName);
			}
			obs_data_array_push_back(audio_names, item);
		}
		obs_data_set_array(s, "audio_names", audio_names);
		if (obs_data_get_bool(settings, "save_file")) {
			std::string path = obs_data_get_string(settings, "directory");
			char lastChar = path.empty() ? '\0' : path.back();
			if (lastChar != '/' && lastChar != '\\') {
				path += "/";
			}
			auto extension = obs_data_get_string(settings, "extension");
			bool spaces = obs_data_get_bool(settings, "allow_spaces");
			auto format = obs_data_get_string(settings, "format");
			obs_data_set_string(s, "directory", path.c_str());
			obs_data_set_string(s, "path", path.c_str());
			obs_data_set_string(s, "format", format);
			obs_data_set_string(s, "extension", extension);
			obs_data_set_bool(s, "allow_spaces", spaces);
			obs_data_set_bool(s, "allow_overwrite", obs_data_get_bool(settings, "allow_overwrite"));
			obs_data_set_bool(s, "split_file", true);
			obs_data_set_int(s, "max_time_sec", obs_data_get_int(settings, "max_time_sec"));
			obs_data_set_int(s, "max_size_mb", obs_data_get_int(settings, "max_size_mb"));

			//ensure_directory_exists(strPath);

			auto filename = os_generate_formatted_filename(extension, spaces, format);
			path += filename;
			obs_data_set_string(s, "url", path.c_str());
			bfree(filename);
		} else {
			obs_data_set_string(s, "url", obs_data_get_string(settings, "url"));
		}

		obs_data_set_string(s, "format_name", obs_data_get_string(settings, "format_name"));
		obs_data_set_string(s, "format_mime_type", obs_data_get_string(settings, "format_mime_type"));
		obs_data_set_string(s, "muxer_settings", obs_data_get_string(settings, "muxer_settings"));
		obs_data_set_int(s, "gop_size", obs_data_get_int(settings, "gop_size"));
		obs_data_set_int(s, "video_bitrate", obs_data_get_int(settings, "video_bitrate"));
		obs_data_set_string(s, "video_encoder", obs_data_get_string(settings, "video_encoder"));
		obs_data_set_int(s, "video_encoder_id", obs_data_get_int(settings, "video_encoder_id"));
		obs_data_set_string(s, "video_settings", obs_data_get_string(settings, "video_settings"));
		obs_data_set_int(s, "audio_bitrate", obs_data_get_int(settings, "audio_bitrate"));
		obs_data_set_string(s, "audio_encoder", obs_data_get_string(settings, "audio_encoder"));
		obs_data_set_int(s, "audio_encoder_id", obs_data_get_int(settings, "audio_encoder_id"));
		obs_data_set_string(s, "audio_settings", obs_data_get_string(settings, "audio_settings"));
		if (obs_data_get_bool(settings, "rescale")) {
			obs_data_set_int(s, "scale_width", obs_data_get_int(settings, "scale_width"));
			obs_data_set_int(s, "scale_height", obs_data_get_int(settings, "scale_height"));
		}

		size_t aMixes = obs_data_get_int(settings, "audio_mixes");
		obs_output_set_mixers(output_, aMixes);
		obs_output_set_media(output_, obs_canvas_get_video(canvas), obs_get_audio());
		obs_canvas_release(canvas);

		obs_output_update(output_, s);

		signal_handler_t *signal = obs_output_get_signal_handler(output_);
		signal_handler_disconnect(signal, "start", output_start, this);
		signal_handler_disconnect(signal, "stop", output_stop, this);
		signal_handler_connect(signal, "start", output_start, this);
		signal_handler_connect(signal, "stop", output_stop, this);
		signal_handler_connect(signal, "reconnect", output_reconnect, this);
		signal_handler_connect(signal, "reconnect_success", output_reconnect_success, this);

		if (!obs_output_start(output_)) {
			obs_output_release(output_);
			output_ = nullptr;
			return false;
		}
		return true;
	}

	std::vector<obs_encoder_t *> vencs;
	std::vector<obs_encoder_t *> aencs;

	//obs_encoder_t *venc = nullptr;
	bool is_record = (strcmp(output_type, "record") == 0 || strcmp(output_type, "backtrack") == 0);
	auto advanced = obs_data_get_bool(settings, "advanced");
	auto video_encoders = obs_data_get_array(settings, "video_encoders");
	auto video_encoders_count = obs_data_array_count(video_encoders);
	if (video_encoders_count == 0) {
		auto venc = videoEncoder(settings, advanced, is_record, name);
		if (venc) {
			vencs.push_back(venc);
		}
	} else {
		for (size_t i = 0; i < video_encoders_count; i++) {
			auto item = obs_data_array_item(video_encoders, i);
			if (!item) {
				continue;
			}
			auto venc = videoEncoder(item, advanced, is_record, name);
			if (venc) {
				vencs.push_back(venc);
			}
			obs_data_release(item);
		}
	}
	obs_data_array_release(video_encoders);

	if (advanced) {
		auto aenc_name = obs_data_get_string(settings, "audio_encoder");
		if (!aenc_name || aenc_name[0] == '\0') {
			//use main encoder
			obs_output_t *main_output = nullptr;
			if (is_record) {
				main_output = obs_frontend_get_replay_buffer_output();
				if (main_output && !obs_output_active(main_output)) {
					obs_output_release(main_output);
					main_output = nullptr;
				}
				if (!main_output) {
					main_output = obs_frontend_get_recording_output();
				}
				if (main_output && !obs_output_active(main_output)) {
					obs_output_release(main_output);
					main_output = nullptr;
				}
			}
			if (!main_output) {
				main_output = obs_frontend_get_streaming_output();
			}

			if (!obs_output_active(main_output)) {
				obs_output_release(main_output);
				blog(LOG_WARNING, "[Fiji Suite] failed to start output '%s' because main was not started",
				     name);
				problem_ = QStringLiteral("MainOutputNotActive");
				return false;
			}
			auto aenc = obs_output_get_audio_encoder(main_output, 0);
			obs_output_release(main_output);
			if (!aenc) {
				blog(LOG_WARNING,
				     "[Fiji Suite] failed to start output '%s' because audio encoder was not found", name);
				problem_ = QStringLiteral("MainOutputEncoderIndexNotFound");
				return false;
			} else {
				obs_encoder_get_ref(aenc);
			}
			aencs.push_back(aenc);
		} else {
			auto streamServer = QString::fromUtf8(obs_data_get_string(settings, "stream_server"));
			if (is_record || streamServer.startsWith("srt://", Qt::CaseInsensitive) ||
			    streamServer.startsWith("rist://", Qt::CaseInsensitive)) {
				auto tracks = obs_data_get_int(settings, "audio_tracks");
				if (!tracks) {
					tracks = 1;
				}

				for (size_t i = 0; i < MAX_AUDIO_MIXES; i++) {
					if ((tracks & (1ll << i)) == 0) {
						continue;
					}
					obs_data_t *s = nullptr;
					auto aes = obs_data_get_obj(settings, "audio_encoder_settings");
					if (aes) {
						s = obs_data_create();
						obs_data_apply(s, aes);
						obs_data_release(aes);
					}

					std::string audio_encoder_name = "Fiji Suite Audio ";
					audio_encoder_name += name;
					audio_encoder_name += " ";
					audio_encoder_name += std::to_string(i + 1);
					auto aenc = obs_audio_encoder_create(aenc_name, audio_encoder_name.c_str(), s, i, nullptr);
					obs_data_release(s);
					obs_encoder_set_audio(aenc, obs_get_audio());
					aencs.push_back(aenc);
				}
			} else {

				obs_data_t *s = nullptr;
				auto aes = obs_data_get_obj(settings, "audio_encoder_settings");
				if (aes) {
					s = obs_data_create();
					obs_data_apply(s, aes);
				}

				std::string audio_encoder_name = "Fiji Suite Audio ";
				audio_encoder_name += name;
				auto audio_track = obs_data_get_int(settings, "audio_track");
				auto aenc =
					obs_audio_encoder_create(aenc_name, audio_encoder_name.c_str(), s, audio_track, nullptr);
				obs_data_release(s);
				obs_encoder_set_audio(aenc, obs_get_audio());
				aencs.push_back(aenc);

				obs_data_set_default_int(settings, "vod_track", -1);
				auto vod_track = obs_data_get_int(settings, "vod_track");
				if (vod_track >= 0 && vod_track != audio_track &&
				    (config_get_bool(obs_frontend_get_user_config(), "General", "EnableCustomServerVodTrack") ||
				     isTwitchServer(streamServer))) {
					if (aes) {
						s = obs_data_create();
						obs_data_apply(s, aes);
					}
					audio_encoder_name = "Fiji Suite VOD Audio ";
					audio_encoder_name += name;
					aenc = obs_audio_encoder_create(aenc_name, audio_encoder_name.c_str(), s, vod_track,
									nullptr);
					obs_data_release(s);
					obs_encoder_set_audio(aenc, obs_get_audio());
					aencs.push_back(aenc);
				}
				obs_data_release(aes);
			}
		}
	} else {
		if (aencs.empty()) {
			obs_output_t *main_output = nullptr;
			if (is_record) {
				main_output = obs_frontend_get_replay_buffer_output();
				if (main_output && !obs_output_active(main_output)) {
					obs_output_release(main_output);
					main_output = nullptr;
				}
				if (!main_output) {
					main_output = obs_frontend_get_recording_output();
				}
				if (main_output && !obs_output_active(main_output)) {
					obs_output_release(main_output);
					main_output = nullptr;
				}
				if (!main_output) {
					main_output = obs_frontend_get_streaming_output();
				}

				for (size_t idx = 0; idx < MAX_OUTPUT_AUDIO_ENCODERS; idx++) {
					auto aenc = main_output ? obs_output_get_audio_encoder(main_output, idx) : nullptr;
					if (aenc) {
						obs_encoder_get_ref(aenc);
						aencs.push_back(aenc);
					}
				}
			} else {
				main_output = obs_frontend_get_streaming_output();
				auto aenc = main_output ? obs_output_get_audio_encoder(main_output, 0) : nullptr;
				if (aenc) {
					obs_encoder_get_ref(aenc);
					aencs.push_back(aenc);
				}
			}

			obs_output_release(main_output);
		}
		if (!vencs.empty() && aencs.empty()) {
			if (is_record) {
				for (size_t i = 0; i < MAX_AUDIO_MIXES; i++) {
					std::string audio_encoder_name = "Fiji Suite Audio ";
					audio_encoder_name += name;
					audio_encoder_name += " ";
					audio_encoder_name += std::to_string(i);
					auto aenc = obs_audio_encoder_create("ffmpeg_aac", audio_encoder_name.c_str(), nullptr, i,
									     nullptr);
					obs_encoder_set_audio(aenc, obs_get_audio());
					aencs.push_back(aenc);
				}
			} else {
				std::string audio_encoder_name = "Fiji Suite Audio ";
				audio_encoder_name += name;
				auto aenc = obs_audio_encoder_create("ffmpeg_aac", audio_encoder_name.c_str(), nullptr, 0, nullptr);
				obs_encoder_set_audio(aenc, obs_get_audio());
				aencs.push_back(aenc);
			}
		}
	}

	if (vencs.empty()) {
		blog(LOG_WARNING, "[Fiji Suite] Failed to start '%s', no video encoder found", name);
		problem_ = QStringLiteral("NoVideoEncoder");
		for (size_t i = 0; i < aencs.size(); i++) {
			obs_encoder_release(aencs[i]);
		}
		return false;
	}
	if (aencs.empty()) {
		blog(LOG_WARNING, "[Fiji Suite] Failed to start '%s', no audio encoder found", name);
		problem_ = QStringLiteral("NoAudioEncoder");
		for (size_t i = 0; i < vencs.size(); i++) {
			obs_encoder_release(vencs[i]);
		}
		return false;
	}

	if (is_record) {
		std::string filenameFormat = obs_data_get_string(settings, "filename");
		if (filenameFormat.empty()) {
			filenameFormat = "%CCYY-%MM-%DD %hh-%mm-%ss";
			filenameFormat += " ";
			QString safeName = QString::fromUtf8(name);
#ifdef __APPLE__
			safeName.replace(QRegularExpression("[:]"), "");
#elif defined(_WIN32)
			safeName.replace(QRegularExpression("[<>:\"\\|\\?\\*]"), "");
#else
			// TODO: Add filtering for other platforms
#endif
			filenameFormat += safeName.toStdString();
		}
		auto format = obs_data_get_string(settings, "format");
		if (format[0] == '\0') {
			format = "hybrid_mp4";
		}
		std::string ext = format;
		if (ext == "fragmented_mp4" || ext == "hybrid_mp4") {
			ext = "mp4";
		} else if (ext == "fragmented_mov" || ext == "hybrid_mov") {
			ext = "mov";
		} else if (ext == "hls") {
			ext = "m3u8";
		} else if (ext == "mpegts") {
			ext = "ts";
		}

		char *filename = os_generate_formatted_filename(ext.c_str(), true, filenameFormat.c_str());

		auto dir = obs_data_get_string(settings, "path");
		char path[512];
		if (dir[0] != '\0') {
			snprintf(path, 512, "%s/%s", dir, filename);
			ensure_directory(path);
		} else {
			snprintf(path, 512, "%s", filename);
		}
		bfree(filename);

		std::string output_name = "Fiji Suite Output ";
		output_name += name;
		const char *output_id = "ffmpeg_muxer";
		if (strcmp(output_type, "backtrack") == 0) {
			output_id = "replay_buffer";
		} else if (strcmp(format, "hybrid_mp4") == 0) {
			output_id = "mp4_output";
		} else if (strcmp(format, "hybrid_mov") == 0) {
			output_id = "mov_output";
		} else if (strcmp(format, "flv") == 0) {
			output_id = "flv_output";
		}

		output_ = obs_output_create(output_id, output_name.c_str(), nullptr, nullptr);

		auto ps = obs_data_create();
		obs_data_set_string(ps, "path", path);
		obs_data_set_string(ps, "directory", dir);
		obs_data_set_string(ps, "format", filenameFormat.c_str());
		obs_data_set_string(ps, "extension", ext.c_str());
		obs_data_set_bool(ps, "split_file", true);
		auto max_size = obs_data_get_int(settings, "max_size_mb");
		obs_data_set_int(ps, "max_size_mb", max_size > 0 ? max_size : 0);
		auto max_sec = obs_data_get_int(settings, "max_time_sec");
		obs_data_set_int(ps, "max_time_sec", max_sec > 0 ? max_sec : 0);
		obs_output_update(output_, ps);
		obs_data_release(ps);
	} else {

		auto server = obs_data_get_string(settings, "stream_server");
		if (!server || !strlen(server)) {
			server = obs_data_get_string(settings, "server");
			if (server && strlen(server)) {
				obs_data_set_string(settings, "stream_server", server);
			}
		}
		bool whip = strstr(server, "whip") != nullptr;
		auto s = obs_data_create();
		obs_data_set_string(s, "server", server);
		auto key = obs_data_get_string(settings, "stream_key");
		if (!key || !strlen(key)) {
			key = obs_data_get_string(settings, "key");
			if (key && strlen(key)) {
				obs_data_set_string(settings, "stream_key", key);
			}
		}
		if (whip) {
			obs_data_set_string(s, "bearer_token", key);
		} else {
			obs_data_set_string(s, "key", key);
		}
		//use_auth
		//username
		//password
		std::string service_name = "fiji_suite_service_";
		service_name += name;
		auto service = obs_service_create(whip ? "whip_custom" : "rtmp_custom", service_name.c_str(), s, nullptr);
		obs_data_release(s);

		const char *type = obs_service_get_preferred_output_type(service);
		if (!type) {
			type = "rtmp_output";
			if (strncmp(server, "ftl", 3) == 0) {
				type = "ftl_output";
			} else if (strncmp(server, "rtmp", 4) != 0) {
				type = "ffmpeg_mpegts_muxer";
			}
		}
		std::string output_name = "Fiji Suite Output ";
		output_name += name;
		output_ = obs_output_create(type, output_name.c_str(), nullptr, nullptr);
		obs_output_set_service(output_, service);

		bool customDelay = obs_data_get_bool(settings, "custom_delay");
		if (customDelay) {
			auto delaySec = (uint32_t)obs_data_get_int(settings, "delay_sec");
			bool preserveDelay = obs_data_get_bool(settings, "delay_preserve");
			obs_output_set_delay(output_, delaySec, preserveDelay ? OBS_OUTPUT_DELAY_PRESERVE : 0);
		}

		config_t *config = obs_frontend_get_profile_config();
		if (config) {
			obs_data_t *output_settings = obs_data_create();
			obs_data_set_string(output_settings, "bind_ip", config_get_string(config, "Output", "BindIP"));
			obs_data_set_string(output_settings, "ip_family", config_get_string(config, "Output", "IPFamily"));
			obs_output_update(output_, output_settings);
			obs_data_release(output_settings);

			if (!customDelay) {
				bool useDelay = config_get_bool(config, "Output", "DelayEnable");
				auto delaySec = (uint32_t)config_get_int(config, "Output", "DelaySec");
				bool preserveDelay = config_get_bool(config, "Output", "DelayPreserve");
				obs_output_set_delay(output_, useDelay ? delaySec : 0,
						     preserveDelay ? OBS_OUTPUT_DELAY_PRESERVE : 0);
			}
		}
	}

	signal_handler_t *signal = obs_output_get_signal_handler(output_);
	signal_handler_disconnect(signal, "start", output_start, this);
	signal_handler_disconnect(signal, "stop", output_stop, this);
	if (isBacktrack_) {
		signal_handler_disconnect(signal, "saved", replay_saved, this);
	}
	signal_handler_connect(signal, "start", output_start, this);
	signal_handler_connect(signal, "stop", output_stop, this);
	signal_handler_connect(signal, "reconnect", output_reconnect, this);
	signal_handler_connect(signal, "reconnect_success", output_reconnect_success, this);
	if (isBacktrack_) {
		signal_handler_connect(signal, "saved", replay_saved, this);
	}

	for (size_t i = 0; i < vencs.size(); i++) {
		obs_output_set_video_encoder2(output_, vencs[i], i);
		obs_encoder_release(vencs[i]);
	}

	for (size_t i = 0; i < aencs.size(); i++) {
		obs_output_set_audio_encoder(output_, aencs[i], i);
		obs_encoder_release(aencs[i]);
	}

	if (!obs_output_start(output_)) {
		obs_output_release(output_);
		output_ = nullptr;
		return false;
	}
	return true;
}

obs_encoder_t *OutputRunner::videoEncoder(obs_data_t *settings, bool advanced, bool is_record, const char *output_name)
{
	const char *canvas_name = obs_data_get_string(settings, "canvas");
	auto canvas = (!canvas_name || canvas_name[0] == '\0') ? obs_get_main_canvas() : obs_get_canvas_by_name(canvas_name);
	if (!canvas) {
		blog(LOG_WARNING, "[Fiji Suite] canvas '%s' not found", canvas_name);
		problem_ = QStringLiteral("CanvasNotFound");
		return nullptr;
	}
	auto main_canvas = obs_get_main_canvas();
	bool main = canvas == main_canvas;
	obs_canvas_release(main_canvas);
	obs_canvas_release(canvas);

	obs_encoder_t *venc = nullptr;
	if (advanced) {
		auto output_video_encoder_name = obs_data_get_string(settings, "output_video_encoder");
		if (output_video_encoder_name && output_video_encoder_name[0] != '\0' &&
		    (!main || strcmp(output_video_encoder_name, "MainEncoder") != 0)) {
			std::string other_output_name = "Fiji Suite Output ";
			other_output_name += output_video_encoder_name;
			auto other_output = obs_get_output_by_name(other_output_name.c_str());
			if (other_output) {
				auto vei = (int)obs_data_get_int(settings, "video_encoder_index");
				venc = obs_output_get_video_encoder2(other_output, vei);
				if (!venc) {
					venc = obs_output_get_video_encoder(other_output);
				}
				obs_output_release(other_output);
			}
			if (!venc) {
				blog(LOG_WARNING, "[Fiji Suite] failed to start output '%s' because '%s' was not started",
				     output_name, output_video_encoder_name);
				problem_ = QStringLiteral("OtherOutputNotActive");
				return nullptr;
			} else {
				obs_encoder_get_ref(venc);
			}
		} else {
			auto venc_name = obs_data_get_string(settings, "video_encoder");
			if (!venc_name || venc_name[0] == '\0') {
				if (main) {
					//use main encoder
					obs_output_t *main_output = nullptr;
					if (is_record) {
						main_output = obs_frontend_get_replay_buffer_output();
						if (main_output && !obs_output_active(main_output)) {
							obs_output_release(main_output);
							main_output = nullptr;
						}
						if (!main_output) {
							main_output = obs_frontend_get_recording_output();
						}
						if (main_output && !obs_output_active(main_output)) {
							obs_output_release(main_output);
							main_output = nullptr;
						}
					}
					if (!main_output) {
						main_output = obs_frontend_get_streaming_output();
					}

					if (!obs_output_active(main_output)) {
						obs_output_release(main_output);
						blog(LOG_WARNING,
						     "[Fiji Suite] failed to start output '%s' because main was not started",
						     output_name);
						problem_ = QStringLiteral("MainOutputNotActive");
						return nullptr;
					}
					auto vei = (int)obs_data_get_int(settings, "video_encoder_index");
					venc = obs_output_get_video_encoder2(main_output, vei);
					obs_output_release(main_output);
					if (!venc) {
						blog(LOG_WARNING,
						     "[Fiji Suite] failed to start output '%s' because encoder index %d was not found",
						     output_name, vei);
						problem_ = QStringLiteral("MainOutputEncoderIndexNotFound");
						return nullptr;
					} else {
						obs_encoder_get_ref(venc);
					}
				} else {
					//default encoder
					std::pair<obs_encoder_t **, video_t *> d = {&venc, obs_canvas_get_video(canvas)};
					obs_enum_outputs(
						[](void *param, obs_output_t *candidate) {
							uint32_t has_flags = OBS_OUTPUT_VIDEO |
									     OBS_OUTPUT_ENCODED; //| OBS_OUTPUT_SERVICE;
							uint32_t flags = obs_output_get_flags(candidate);
							if ((flags & has_flags) != has_flags) {
								return true;
							}

							std::pair<obs_encoder_t **, video_t *> *d =
								(std::pair<obs_encoder_t **, video_t *> *)param;

							for (size_t idx = 0; idx < MAX_OUTPUT_VIDEO_ENCODERS; idx++) {
								auto enc = obs_output_get_video_encoder2(candidate, idx);
								if (enc && obs_encoder_parent_video(enc) == d->second) {
									*d->first = enc;
									return false;
								}
							}
							return true;
						},
						&d);

					if (!venc) {
						auto videoEncoderIds = {"obs_nvenc_h264_tex", "jim_nvenc", "ffmpeg_nvenc",
									"obs_qsv11_v2", "h264_texture_amf"};
						const char *vencid = "obs_x264";
						for (auto &videoEncoderId : videoEncoderIds) {
							if (encoder_available(videoEncoderId)) {
								vencid = videoEncoderId;
								break;
							}
						}
						std::string venc_name = "Fiji Suite Video ";
						venc_name += output_name;
						venc_name += " ";
						venc_name += obs_canvas_get_name(canvas);
						venc = obs_video_encoder_create(vencid, venc_name.c_str(), nullptr, nullptr);
						obs_encoder_set_video(venc, obs_canvas_get_video(canvas));
						auto video_settings = obs_data_create();
						obs_data_set_string(video_settings, "rate_control", "CBR");
						obs_data_set_int(video_settings, "bitrate", 6000);
						obs_encoder_update(venc, video_settings);
						obs_data_release(video_settings);
					} else {
						obs_encoder_get_ref(venc);
					}
				}
			} else {
				obs_data_t *s = nullptr;
				auto ves = obs_data_get_obj(settings, "video_encoder_settings");
				if (ves) {
					s = obs_data_create();
					obs_data_apply(s, ves);
					obs_data_release(ves);
				}
				std::string video_encoder_name = "Fiji Suite Video ";
				video_encoder_name += output_name;
				video_encoder_name += " ";
				video_encoder_name += obs_canvas_get_name(canvas);
				venc = obs_video_encoder_create(venc_name, video_encoder_name.c_str(), s, nullptr);
				obs_data_release(s);
				obs_encoder_set_video(venc, obs_canvas_get_video(canvas));
				auto divisor = obs_data_get_int(settings, "frame_rate_divisor");
				if (divisor > 1) {
					obs_encoder_set_frame_rate_divisor(venc, (uint32_t)divisor);
				}

				bool scale = obs_data_get_bool(settings, "scale");
				if (scale) {
					obs_encoder_set_scaled_size(venc, (uint32_t)obs_data_get_int(settings, "width"),
								    (uint32_t)obs_data_get_int(settings, "height"));
					obs_encoder_set_gpu_scale_type(venc,
								       (obs_scale_type)obs_data_get_int(settings, "scale_type"));
				}

				enum video_format video_format = (enum video_format)obs_data_get_int(settings, "color_format");
				obs_encoder_set_preferred_video_format(venc, video_format);

				enum video_colorspace colorspace = (enum video_colorspace)obs_data_get_int(settings, "color_space");
				obs_encoder_set_preferred_color_space(venc, colorspace);

				enum video_range_type range = (enum video_range_type)obs_data_get_int(settings, "color_range");
				obs_encoder_set_preferred_range(venc, range);
			}
		}
	} else {
		std::pair<obs_encoder_t **, video_t *> d = {&venc, obs_canvas_get_video(canvas)};
		obs_enum_outputs(
			[](void *param, obs_output_t *candidate) {
				uint32_t has_flags = OBS_OUTPUT_VIDEO | OBS_OUTPUT_ENCODED; //| OBS_OUTPUT_SERVICE;
				uint32_t flags = obs_output_get_flags(candidate);
				if ((flags & has_flags) != has_flags) {
					return true;
				}

				std::pair<obs_encoder_t **, video_t *> *d = (std::pair<obs_encoder_t **, video_t *> *)param;

				for (size_t idx = 0; idx < MAX_OUTPUT_VIDEO_ENCODERS; idx++) {
					auto enc = obs_output_get_video_encoder2(candidate, idx);
					if (enc && obs_encoder_video(enc) == d->second) {
						*d->first = obs_encoder_get_ref(enc);
						return false;
					}
				}
				return true;
			},
			&d);

		if (!venc && main) {
			obs_output_t *main_output = nullptr;
			if (is_record) {
				main_output = obs_frontend_get_replay_buffer_output();
				if (main_output && !obs_output_active(main_output)) {
					obs_output_release(main_output);
					main_output = nullptr;
				}
				if (!main_output) {
					main_output = obs_frontend_get_recording_output();
				}
				if (main_output && !obs_output_active(main_output)) {
					obs_output_release(main_output);
					main_output = nullptr;
				}
			}
			if (!main_output) {
				main_output = obs_frontend_get_streaming_output();
			}
			venc = main_output ? obs_output_get_video_encoder(main_output) : nullptr;
			obs_output_release(main_output);
			if (!venc || !obs_output_active(main_output)) {
				blog(LOG_WARNING, "[Fiji Suite] failed to start output '%s' because main was not started",
				     output_name);
				problem_ = QStringLiteral("MainOutputNotActive");
				return nullptr;
			}
			obs_encoder_get_ref(venc);
		} else if (!venc) {
			auto videoEncoderIds = {"obs_nvenc_h264_tex", "jim_nvenc", "ffmpeg_nvenc", "obs_qsv11_v2",
						"h264_texture_amf"};
			const char *vencid = "obs_x264";
			for (auto &videoEncoderId : videoEncoderIds) {
				if (encoder_available(videoEncoderId)) {
					vencid = videoEncoderId;
					break;
				}
			}
			std::string venc_name = "Fiji Suite Video ";
			venc_name += output_name;
			venc_name += " ";
			venc_name += obs_canvas_get_name(canvas);
			venc = obs_video_encoder_create(vencid, venc_name.c_str(), nullptr, nullptr);
			obs_encoder_set_video(venc, obs_canvas_get_video(canvas));
			auto video_settings = obs_data_create();
			obs_data_set_string(video_settings, "rate_control", "CBR");
			obs_data_set_int(video_settings, "bitrate", 6000);
			obs_encoder_update(venc, video_settings);
			obs_data_release(video_settings);
		}
	}

	return venc;
}

} // namespace fiji::obs
