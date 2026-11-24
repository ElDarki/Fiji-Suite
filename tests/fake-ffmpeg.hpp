#pragma once

// A small FFmpeg catalogue for the tests: two formats, three encoders, and which format fits which codec.

#include "../src/model/ffmpeg-output.hpp"

struct FakeFfmpeg final : fiji::model::FfmpegCatalogue {
	// mp4: video h264 (27), audio aac (86018); mp3: audio only (86017)
	QList<fiji::model::FfmpegFormat> formats() override
	{
		return {{"mp4", "video/mp4", "MP4 (MPEG-4 Part 14)", "mp4,m4a", 86018, 27, 1},
			{"mp3", "audio/mpeg", "MP3 (MPEG audio layer 3)", "mp3", 86017, 0, 2}};
	}
	QList<fiji::model::FfmpegCodec> encoders() override
	{
		return {{"libx264", "H.264", 27, true}, {"aac", "AAC", 86018, false}, {"libmp3lame", "MP3", 86017, false},
			{"prores", "ProRes", 147, true}};
	}
	bool fits(const fiji::model::FfmpegFormat &format, int codecId) override
	{
		return format.tags == 1 ? (codecId == 27 || codecId == 86018 || codecId == 86017) : codecId == 86017;
	}
};
