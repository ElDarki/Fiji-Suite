#include "obs-stats.hpp"

#include <obs.hpp>

// The colour the user gave a canvas in the plugin's profile settings, or -1 (fiji-suite.cpp).
long long fiji_canvas_color(const char *canvas_name);

namespace fiji::obs {

namespace {

quintptr idOf(const void *p)
{
	return reinterpret_cast<quintptr>(p);
}

model::EncoderSample encoderSample(obs_encoder_t *encoder)
{
	model::EncoderSample s;
	s.id = idOf(encoder);
	s.name = QString::fromUtf8(obs_encoder_get_name(encoder));
	s.video = idOf(obs_encoder_parent_video(encoder));
	s.active = obs_encoder_active(encoder);
	video_t *video = obs_encoder_video(encoder);
	s.width = video ? video_output_get_width(video) : 0;
	s.height = video ? video_output_get_height(video) : 0;
	s.frames = obs_encoder_get_encoded_frames(encoder);
	return s;
}

} // namespace

model::StatsSnapshot ObsStatsSource::sample()
{
	model::StatsSnapshot snapshot;
	obs_enum_encoders(
		[](void *data, obs_encoder_t *encoder) {
			if (obs_encoder_get_type(encoder) == OBS_ENCODER_VIDEO) {
				static_cast<model::StatsSnapshot *>(data)->encoders.append(encoderSample(encoder));
			}
			return true;
		},
		&snapshot);

	obs_enum_outputs(
		[](void *data, obs_output_t *output) {
			model::OutputSample s;
			s.id = idOf(output);
			s.name = QString::fromUtf8(obs_output_get_name(output));
			s.encoded = (obs_output_get_flags(output) & OBS_OUTPUT_ENCODED) != 0;
			s.active = obs_output_active(output);
			if (s.encoded) {
				for (size_t i = 0; i < MAX_OUTPUT_VIDEO_ENCODERS; ++i) {
					if (obs_encoder_t *encoder = obs_output_get_video_encoder2(output, i)) {
						s.videoEncoders.append(encoderSample(encoder));
					}
				}
			} else {
				s.video = idOf(obs_output_video(output));
			}
			s.bytes = obs_output_get_total_bytes(output);
			s.frames = static_cast<quint32>(obs_output_get_total_frames(output));
			s.dropped = static_cast<quint32>(obs_output_get_frames_dropped(output));
			s.delay = obs_output_get_active_delay(output);
			static_cast<model::StatsSnapshot *>(data)->outputs.append(s);
			return true;
		},
		&snapshot);

	obs_enum_canvases(
		[](void *data, obs_canvas_t *canvas) {
			model::CanvasSample s;
			s.id = idOf(canvas);
			const char *name = obs_canvas_get_name(canvas);
			s.name = QString::fromUtf8(name);
			s.color = fiji_canvas_color(name);
			video_t *video = obs_canvas_get_video(canvas);
			s.video = idOf(video);
			if (video) {
				s.frames = video_output_get_total_frames(video);
				s.skipped = video_output_get_skipped_frames(video);
			}
			obs_video_info ovi;
			if (obs_canvas_get_video_info(canvas, &ovi)) {
				s.width = ovi.base_width;
				s.height = ovi.base_height;
			}
			static_cast<model::StatsSnapshot *>(data)->canvases.append(s);
			return true;
		},
		&snapshot);
	return snapshot;
}

} // namespace fiji::obs
