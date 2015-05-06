
#pragma once

#include "./export_import_def.hpp"
#include "stdint.h"

#ifndef ENCODER_API_EXPORT
#define ENCODER_API_IMPORT LIB_SYMBOL_EXPORT
#define ENCODER_API_EXPORT LIB_SYMBOL_EXPORT
#endif

#ifndef BUILD_ENCODER_SOURCE_CODE
#define ENCODER_API ENCODER_API_IMPORT
#else
#define ENCODER_API ENCODER_API_EXPORT
#endif

#ifdef BUILD_STATIC_NO_EXPORT
#undef  ENCODER_API_IMPORT
#undef  ENCODER_API_EXPORT
#define ENCODER_API_IMPORT
#define ENCODER_API_EXPORT
#endif

extern "C"
{
	struct encoder_t;
	ENCODER_API encoder_t* create_encoder(const char* outputfilename, int audio_channel, int audio_sample_rate, int fps, int video_width, int video_height, bool keep_ratio, int clip_top, int clip_bottom, int clip_left, int clip_right);

	ENCODER_API void encoder_feed_audio(encoder_t*, uint8_t* data, long size, int64_t timestamp);
	ENCODER_API void encoder_feed_video_frame(encoder_t*, uint8_t* data, int width, int height, int linesize, int64_t timestamp, bool flip_picture);
	ENCODER_API void encoder_flush_frames(encoder_t*);
	ENCODER_API void encoder_do_benchmark_and_setup_parameters();
	ENCODER_API void destory_encoder(encoder_t* encoder);
}


