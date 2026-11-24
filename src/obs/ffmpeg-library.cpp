#include "ffmpeg-library.hpp"

#include <util/platform.h>

#include <cstdint>

// The few FFmpeg declarations the catalogue reads (the layouts of FFmpeg 7 / 8, libavformat 61-62).
extern "C" {
typedef enum {
	AV_CLASS_CATEGORY_NA = 0,
	AV_CLASS_CATEGORY_INPUT,
	AV_CLASS_CATEGORY_OUTPUT,
	AV_CLASS_CATEGORY_MUXER,
	AV_CLASS_CATEGORY_DEMUXER,
	AV_CLASS_CATEGORY_ENCODER,
	AV_CLASS_CATEGORY_DECODER,
	AV_CLASS_CATEGORY_FILTER,
	AV_CLASS_CATEGORY_BITSTREAM_FILTER,
	AV_CLASS_CATEGORY_SWSCALER,
	AV_CLASS_CATEGORY_SWRESAMPLER,
	AV_CLASS_CATEGORY_DEVICE_VIDEO_OUTPUT = 40,
	AV_CLASS_CATEGORY_DEVICE_VIDEO_INPUT,
	AV_CLASS_CATEGORY_DEVICE_AUDIO_OUTPUT,
	AV_CLASS_CATEGORY_DEVICE_AUDIO_INPUT,
	AV_CLASS_CATEGORY_DEVICE_OUTPUT,
	AV_CLASS_CATEGORY_DEVICE_INPUT,
	AV_CLASS_CATEGORY_NB ///< not part of ABI/API
} AVClassCategory;

typedef struct AVClass {
	const char *class_name;
	const char *(*item_name)(void *ctx);
	const struct AVOption *option;
	int version;
	int log_level_offset_offset;
	int parent_log_context_offset;
	AVClassCategory category;
	AVClassCategory (*get_category)(void *ctx);
	int (*query_ranges)(struct AVOptionRanges **, void *obj, const char *key, int flags);
	void *(*child_next)(void *obj, void *prev);
	const struct AVClass *(*child_class_iterate)(void **iter);
} AVClass;

enum AVCodecID {
	AV_CODEC_ID_NONE,
};

struct AVCodecTag;

typedef struct AVOutputFormat {
	const char *name;
	const char *long_name;
	const char *mime_type;
	const char *extensions;        /**< comma-separated filename extensions */
	enum AVCodecID audio_codec;    /**< default audio codec */
	enum AVCodecID video_codec;    /**< default video codec */
	enum AVCodecID subtitle_codec; /**< default subtitle codec */
	int flags;
	const struct AVCodecTag *const *codec_tag;
	const AVClass *priv_class; ///< AVClass for the private context
} AVOutputFormat;

#ifdef __GNUC__
#define AV_GCC_VERSION_AT_LEAST(x, y) (__GNUC__ > (x) || __GNUC__ == (x) && __GNUC_MINOR__ >= (y))
#define AV_GCC_VERSION_AT_MOST(x, y)  (__GNUC__ < (x) || __GNUC__ == (x) && __GNUC_MINOR__ <= (y))
#else
#define AV_GCC_VERSION_AT_LEAST(x, y) 0
#define AV_GCC_VERSION_AT_MOST(x, y)  0
#endif

#if AV_GCC_VERSION_AT_LEAST(3, 1)
#define attribute_deprecated __attribute__((deprecated))
#elif defined(_MSC_VER)
#define attribute_deprecated __declspec(deprecated)
#else
#define attribute_deprecated
#endif

typedef struct AVRational {
	int num; ///< Numerator
	int den; ///< Denominator
} AVRational;

typedef struct AVProfile {
	int profile;
	const char *name; ///< short name for the profile
} AVProfile;

enum AVMediaType {
	AVMEDIA_TYPE_UNKNOWN = -1, ///< Usually treated as AVMEDIA_TYPE_DATA
	AVMEDIA_TYPE_VIDEO,
	AVMEDIA_TYPE_AUDIO,
	AVMEDIA_TYPE_DATA, ///< Opaque data information usually continuous
	AVMEDIA_TYPE_SUBTITLE,
	AVMEDIA_TYPE_ATTACHMENT, ///< Opaque data information usually sparse
	AVMEDIA_TYPE_NB
};

typedef struct AVCodec {
	const char *name;
	const char *long_name;
	enum AVMediaType type;
	enum AVCodecID id;
	int capabilities;
	uint8_t max_lowres;                                          ///< maximum value for lowres supported by the decoder
	attribute_deprecated const AVRational *supported_framerates; ///< @deprecated use avcodec_get_supported_config()
	attribute_deprecated const void *pix_fmts;                   ///< @deprecated use avcodec_get_supported_config()
	attribute_deprecated const int *supported_samplerates;       ///< @deprecated use avcodec_get_supported_config()
	attribute_deprecated const void *sample_fmts;                ///< @deprecated use avcodec_get_supported_config()
	const AVClass *priv_class;                                   ///< AVClass for the private context
	const AVProfile *profiles; ///< array of recognized profiles, or NULL if unknown, array is terminated by {AV_PROFILE_UNKNOWN}
	const char *wrapper_name;
} AVCodec;

static const AVOutputFormat *(*av_muxer_iterate)(void **opaque) = NULL;
static unsigned int (*av_codec_get_tag)(const struct AVCodecTag *const *tags, enum AVCodecID id) = NULL;
static const AVCodec *(*av_codec_iterate)(void **opaque) = NULL;
static int (*av_codec_is_encoder)(const AVCodec *codec) = NULL;
}

namespace fiji::obs {

namespace {
void *openFirst(const char *const *names)
{
	for (const char *const *name = names; *name; ++name) {
		if (void *handle = os_dlopen(*name)) {
			return handle;
		}
	}
	return nullptr;
}

bool loadFormatLibrary()
{
	if (av_muxer_iterate && av_codec_get_tag) {
		return true;
	}
	static const char *const names[] = {
#ifdef __APPLE__
		"avformat-62.dylib", "avformat-61.dylib",
#endif
		"avformat-62", "avformat-61", nullptr};
	if (void *handle = openFirst(names)) {
		av_muxer_iterate = (const AVOutputFormat *(*)(void **))os_dlsym(handle, "av_muxer_iterate");
		av_codec_get_tag = (unsigned int (*)(const struct AVCodecTag *const *, enum AVCodecID))os_dlsym(handle, "av_codec_get_tag");
		os_dlclose(handle);
	}
	return av_muxer_iterate && av_codec_get_tag;
}

bool loadCodecLibrary()
{
	if (av_codec_iterate && av_codec_is_encoder) {
		return true;
	}
	static const char *const names[] = {
#ifdef __APPLE__
		"avcodec-62.dylib", "avcodec-61.dylib",
#endif
		"avcodec-62", "avcodec-61", nullptr};
	if (void *handle = openFirst(names)) {
		av_codec_iterate = (const AVCodec *(*)(void **))os_dlsym(handle, "av_codec_iterate");
		av_codec_is_encoder = (int (*)(const AVCodec *))os_dlsym(handle, "av_codec_is_encoder");
		os_dlclose(handle);
	}
	return av_codec_iterate && av_codec_is_encoder;
}

QString text(const char *value)
{
	return value ? QString::fromUtf8(value) : QString();
}
} // namespace

QList<model::FfmpegFormat> FfmpegLibrary::formats()
{
	QList<model::FfmpegFormat> out;
	if (!loadFormatLibrary()) {
		return out;
	}
	void *i = nullptr;
	while (const AVOutputFormat *format = av_muxer_iterate(&i)) {
		if (format->priv_class && (format->priv_class->category == AV_CLASS_CATEGORY_DEVICE_VIDEO_OUTPUT ||
					   format->priv_class->category == AV_CLASS_CATEGORY_DEVICE_AUDIO_OUTPUT ||
					   format->priv_class->category == AV_CLASS_CATEGORY_DEVICE_OUTPUT)) {
			continue;
		}
		if (format->audio_codec == AV_CODEC_ID_NONE && format->video_codec == AV_CODEC_ID_NONE) {
			continue;
		}
		out.append({text(format->name), text(format->mime_type), text(format->long_name), text(format->extensions),
			    int(format->audio_codec), int(format->video_codec), quintptr(format->codec_tag)});
	}
	return out;
}

QList<model::FfmpegCodec> FfmpegLibrary::encoders()
{
	QList<model::FfmpegCodec> out;
	if (!loadCodecLibrary()) {
		return out;
	}
	void *i = nullptr;
	while (const AVCodec *codec = av_codec_iterate(&i)) {
		if (!av_codec_is_encoder(codec) || (codec->type != AVMEDIA_TYPE_VIDEO && codec->type != AVMEDIA_TYPE_AUDIO)) {
			continue;
		}
		out.append({text(codec->name), text(codec->long_name), int(codec->id), codec->type == AVMEDIA_TYPE_VIDEO});
	}
	return out;
}

bool FfmpegLibrary::fits(const model::FfmpegFormat &format, int codecId)
{
	return loadFormatLibrary() &&
	       av_codec_get_tag(reinterpret_cast<const struct AVCodecTag *const *>(format.tags), AVCodecID(codecId)) != 0;
}

} // namespace fiji::obs
