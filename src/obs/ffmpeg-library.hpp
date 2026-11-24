#pragma once

// model::FfmpegCatalogue from the FFmpeg libraries OBS ships (avformat / avcodec, opened at run time, so the plugin
// does not link against them).

#include "../model/ffmpeg-output.hpp"

namespace fiji::obs {

class FfmpegLibrary final : public model::FfmpegCatalogue {
public:
	QList<model::FfmpegFormat> formats() override;
	QList<model::FfmpegCodec> encoders() override;
	bool fits(const model::FfmpegFormat &format, int codecId) override;
};

} // namespace fiji::obs
