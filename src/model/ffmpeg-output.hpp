#pragma once

// MODEL: a Fiji Suite FFmpeg output (to a file or a URL) as its dialog edits it, the FFmpeg formats and encoders it
// can choose from (FfmpegCatalogue: FFmpeg's own lists, which obs::FfmpegLibrary reads), and the dialog's rules.
// Stored in the profile's output list with the keys toJson writes. Qt Core only.

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

namespace fiji::model {

struct FfmpegOutput {
	QString name;
	bool saveFile = true; // a file in `directory`, else `url`
	QString directory;
	QString extension;
	QString filename; // OBS's filename formatting
	bool allowSpaces = true;
	QString url;
	QString formatName;
	QString mimeType;
	QString muxerSettings;
	int gopSize = 250;
	int videoBitrate = 2500;
	bool rescale = false;
	int scaleWidth = 0, scaleHeight = 0;
	QString videoEncoder;
	int videoEncoderId = 0;
	QString videoSettings;
	int audioBitrate = 128;
	QString audioEncoder;
	int audioEncoderId = 0;
	QString audioSettings;
	unsigned audioMixes = 1; // a bit per track

	static FfmpegOutput fromJson(const QJsonObject &json);
	QJsonObject toJson() const;
};

struct FfmpegFormat {
	QString name, mimeType, longName;
	QString extensions; // comma-separated
	int audioCodec = 0, videoCodec = 0; // 0 = none
	quintptr tags = 0;                  // the catalogue's handle for codec compatibility
};

struct FfmpegCodec {
	QString name, longName;
	int id = 0;
	bool video = false;
};

class FfmpegCatalogue {
public:
	virtual ~FfmpegCatalogue() = default;
	// Muxers that write audio and/or video (no devices).
	virtual QList<FfmpegFormat> formats() = 0;
	virtual QList<FfmpegCodec> encoders() = 0;
	// Whether the format can hold the codec.
	virtual bool fits(const FfmpegFormat &format, int codecId) = 0;
};

// An encoder the dialog lists, and whether it is the format's default.
struct FfmpegEncoderChoice {
	FfmpegCodec codec;
	bool isDefault = false;
	// "name - long name"
	QString label() const;
};
struct FfmpegEncoderChoices {
	QList<FfmpegEncoderChoice> video, audio; // sorted by label
};
// The encoders for a format: those it fits (or all, when compatibility is ignored), plus its defaults.
FfmpegEncoderChoices ffmpegEncodersFor(FfmpegCatalogue &catalogue, const FfmpegFormat &format, bool ignoreCompatibility);

// The first of a format's extensions ("mp4,m4a" -> "mp4").
QString firstExtension(const QString &extensions);
// "1280x720" -> 1280, 720; false when it is not that shape.
bool parseScale(const QString &text, int &width, int &height);
// Whether the dialog may save: a name no other output has, a format, and a folder, extension and filename (to a file)
// or a URL.
bool ffmpegOutputValid(const FfmpegOutput &output, const QStringList &otherNames);

} // namespace fiji::model
