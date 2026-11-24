#include "obs-stats-source.hpp"

#include <obs.hpp>

namespace fiji::obs {

model::StatsSnapshot ObsStatsSource::sample()
{
	model::StatsSnapshot snapshot;
	obs_enum_encoders(
		[](void *data, obs_encoder_t *encoder) {
			auto out = static_cast<model::StatsSnapshot *>(data);
			model::StatsEncoder e;
			e.id = quintptr(encoder);
			e.name = QString::fromUtf8(obs_encoder_get_name(encoder));
			e.isVideo = obs_encoder_get_type(encoder) == OBS_ENCODER_VIDEO;
			e.active = obs_encoder_active(encoder);
			if (e.isVideo) {
				e.video = quintptr(obs_encoder_parent_video(encoder));
				e.encodedFrames = obs_encoder_get_encoded_frames(encoder);
				if (video_t *video = obs_encoder_video(encoder)) {
					e.width = video_output_get_width(video);
					e.height = video_output_get_height(video);
				}
			}
			out->encoders.append(e);
			return true;
		},
		&snapshot);
	obs_enum_outputs(
		[](void *data, obs_output_t *output) {
			auto out = static_cast<model::StatsSnapshot *>(data);
			model::StatsOutput o;
			o.id = quintptr(output);
			o.name = QString::fromUtf8(obs_output_get_name(output));
			o.active = obs_output_active(output);
			o.encoded = obs_output_get_flags(output) & OBS_OUTPUT_ENCODED;
			if (o.encoded) {
				for (size_t i = 0; i < MAX_OUTPUT_VIDEO_ENCODERS; ++i) {
					if (obs_encoder_t *encoder = obs_output_get_video_encoder2(output, i)) {
						o.videoEncoders.append(quintptr(encoder));
					}
				}
			} else {
				o.video = quintptr(obs_output_video(output));
			}
			o.activeDelay = obs_output_get_active_delay(output);
			o.droppedFrames = obs_output_get_frames_dropped(output);
			o.totalBytes = obs_output_get_total_bytes(output);
			o.totalFrames = obs_output_get_total_frames(output);
			out->outputs.append(o);
			return true;
		},
		&snapshot);
	obs_enum_canvases(
		[](void *data, obs_canvas_t *canvas) {
			auto out = static_cast<model::StatsSnapshot *>(data);
			video_t *video = obs_canvas_get_video(canvas);
			if (!video) {
				return true;
			}
			model::StatsCanvas c;
			c.name = QString::fromUtf8(obs_canvas_get_name(canvas));
			c.video = quintptr(video);
			c.skippedFrames = video_output_get_skipped_frames(video);
			c.totalFrames = video_output_get_total_frames(video);
			struct obs_video_info ovi;
			if (obs_canvas_get_video_info(canvas, &ovi)) {
				c.width = ovi.base_width;
				c.height = ovi.base_height;
			}
			out->canvases.append(c);
			return true;
		},
		&snapshot);
	return snapshot;
}

} // namespace fiji::obs
