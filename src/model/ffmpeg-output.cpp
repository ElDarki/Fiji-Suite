#include "ffmpeg-output.hpp"

#include <QRegularExpression>
#include <algorithm>

namespace fiji::model {

FfmpegOutput FfmpegOutput::fromJson(const QJsonObject &j)
{
	FfmpegOutput o;
	o.name = j.value("name").toString();
	o.saveFile = j.value("save_file").toBool(o.saveFile);
	o.directory = j.value("directory").toString();
	o.extension = j.value("extension").toString();
	o.filename = j.value("format").toString();
	o.allowSpaces = j.value("allow_spaces").toBool(o.allowSpaces);
	o.url = j.value("url").toString();
	o.formatName = j.value("format_name").toString();
	o.mimeType = j.value("format_mime_type").toString();
	o.muxerSettings = j.value("muxer_settings").toString();
	o.gopSize = j.value("gop_size").toInt(o.gopSize);
	o.videoBitrate = j.value("video_bitrate").toInt(o.videoBitrate);
	o.rescale = j.value("rescale").toBool();
	o.scaleWidth = j.value("scale_width").toInt();
	o.scaleHeight = j.value("scale_height").toInt();
	o.videoEncoder = j.value("video_encoder").toString();
	o.videoEncoderId = j.value("video_encoder_id").toInt();
	o.videoSettings = j.value("video_settings").toString();
	o.audioBitrate = j.value("audio_bitrate").toInt(o.audioBitrate);
	o.audioEncoder = j.value("audio_encoder").toString();
	o.audioEncoderId = j.value("audio_encoder_id").toInt();
	o.audioSettings = j.value("audio_settings").toString();
	o.audioMixes = unsigned(j.value("audio_mixes").toInt(int(o.audioMixes)));
	return o;
}

QJsonObject FfmpegOutput::toJson() const
{
	return {{"name", name},
		{"save_file", saveFile},
		{"directory", directory},
		{"extension", extension},
		{"format", filename},
		{"allow_spaces", allowSpaces},
		{"url", url},
		{"format_name", formatName},
		{"format_mime_type", mimeType},
		{"muxer_settings", muxerSettings},
		{"gop_size", gopSize},
		{"video_bitrate", videoBitrate},
		{"rescale", rescale},
		{"scale_width", scaleWidth},
		{"scale_height", scaleHeight},
		{"video_encoder", videoEncoder},
		{"video_encoder_id", videoEncoderId},
		{"video_settings", videoSettings},
		{"audio_bitrate", audioBitrate},
		{"audio_encoder", audioEncoder},
		{"audio_encoder_id", audioEncoderId},
		{"audio_settings", audioSettings},
		{"audio_mixes", int(audioMixes)}};
}

QString FfmpegEncoderChoice::label() const
{
	return codec.longName.isEmpty() ? codec.name : codec.name + QStringLiteral(" - ") + codec.longName;
}

FfmpegEncoderChoices ffmpegEncodersFor(FfmpegCatalogue &catalogue, const FfmpegFormat &format, bool ignoreCompatibility)
{
	FfmpegEncoderChoices out;
	bool videoDefault = false, audioDefault = false;
	for (const auto &codec : catalogue.encoders()) {
		const bool isDefault = codec.id == (codec.video ? format.videoCodec : format.audioCodec) && codec.id != 0;
		if (!ignoreCompatibility && !isDefault && !catalogue.fits(format, codec.id)) {
			continue;
		}
		FfmpegEncoderChoice choice{codec, false};
		// only the first encoder of the default codec is marked as the default
		if (isDefault && codec.video && !videoDefault) {
			choice.isDefault = videoDefault = true;
		} else if (isDefault && !codec.video && !audioDefault) {
			choice.isDefault = audioDefault = true;
		}
		(codec.video ? out.video : out.audio).append(choice);
	}
	auto byLabel = [](const FfmpegEncoderChoice &a, const FfmpegEncoderChoice &b) {
		return a.label() < b.label();
	};
	std::sort(out.video.begin(), out.video.end(), byLabel);
	std::sort(out.audio.begin(), out.audio.end(), byLabel);
	return out;
}

QString firstExtension(const QString &extensions)
{
	return extensions.section(QLatin1Char(','), 0, 0);
}

bool parseScale(const QString &text, int &width, int &height)
{
	static const QRegularExpression shape(QStringLiteral("^(\\d{1,5})x(\\d{1,5})$"));
	const auto match = shape.match(text);
	if (!match.hasMatch()) {
		return false;
	}
	width = match.captured(1).toInt();
	height = match.captured(2).toInt();
	return true;
}

bool ffmpegOutputValid(const FfmpegOutput &o, const QStringList &otherNames)
{
	if (o.name.isEmpty() || otherNames.contains(o.name) || o.formatName.isEmpty()) {
		return false;
	}
	return o.saveFile ? !o.extension.isEmpty() && !o.directory.isEmpty() && !o.filename.isEmpty() : !o.url.isEmpty();
}

} // namespace fiji::model
