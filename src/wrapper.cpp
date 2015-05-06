
#define BUILD_ENCODER_SOURCE_CODE

#if defined(_WIN32) && !defined(_WINDLL)
//#undef LIB_SYMBOL_EXPORT
//#define LIB_SYMBOL_EXPORT
#endif

#include <cstdlib>
#include <sstream>
#include <boost/atomic.hpp>
#include "libencoder_api.hpp"
#include "encoder.hpp"


extern "C"
{
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavfilter/avfilter.h"
#include "libavfilter/buffersrc.h"
#include "libavfilter/buffersink.h"
#include "libavfilter/avfilter.h"
#include "libswscale/swscale.h"
#include "libavutil/time.h"
#include "libavutil/intreadwrite.h"
#include "libavutil/audio_fifo.h"
#include "libswresample/swresample.h"
#include "libavutil/opt.h"
// #include "png.h"
// #include "x264.h"
}


using namespace libencoder;

extern "C" {

ENCODER_API encoder_t* create_encoder(const char* outputfilename, int audio_channel, int audio_sample_rate, int fps, int video_width, int video_height, bool keep_ratio, int clip_top, int clip_bottom, int clip_left, int clip_right)
{
	rect clip_rect;

	clip_rect.top = clip_top;
	clip_rect.bottom = clip_bottom;
	clip_rect.left = clip_left;
	clip_rect.right = clip_right;

#ifdef _WIN32
	std::stringstream ss;
	ss << "clip size: { top: " << clip_rect.top << ", bottom: " << clip_rect.bottom << "left :" << clip_rect.left << "right; " << clip_rect.right << "};";
	OutputDebugStringA(ss.str().c_str());
#endif
	return reinterpret_cast<encoder_t*>(new encoder(outputfilename, audio_channel, audio_sample_rate, fps, video_width, video_height, keep_ratio, clip_rect));
}

ENCODER_API void encoder_feed_audio(encoder_t* _encoder, uint8_t* data, long size, int64_t timestamp)
{
	encoder* _this = reinterpret_cast<encoder*>(_encoder);

	_this->do_audio_frame(data, size, timestamp);
}

ENCODER_API void encoder_feed_video_frame(encoder_t* _encoder, uint8_t* data, int width, int height, int linesize, int64_t timestamp, bool flip_picture)
{
	encoder* _this = reinterpret_cast<encoder*>(_encoder);

	_this->do_video_frame(data, width, height, linesize, timestamp, flip_picture);
}

ENCODER_API void encoder_flush_frames(encoder_t* _encoder)
{
	encoder* _this = reinterpret_cast<encoder*>(_encoder);

	_this->flush_and_write_tailer();
}

ENCODER_API void destory_encoder(encoder_t* _encoder)
{
	delete reinterpret_cast<encoder*>(_encoder);
}

}

namespace {

class ffmpeg_initor
{
public:
	ffmpeg_initor();
	~ffmpeg_initor();
};

#ifdef WIN32
static void avlog_out(void*, int, const char* fmt, va_list v)
{
	char buf[4096];
	vsprintf(buf, fmt, v);

	OutputDebugStringA(buf);
	puts(buf);
}
#endif

ffmpeg_initor::ffmpeg_initor()
{
	av_register_all();
	avcodec_register_all();
	avformat_network_init();
	av_log_set_level(AV_LOG_DEBUG);
#ifdef WIN32
	av_log_set_callback(avlog_out);
	auto user32_model =  GetModuleHandleW(L"user32");
	auto addr = GetProcAddress(user32_model, "SetProcessDPIAware");

	if (addr)
		reinterpret_cast<void (WINAPI *) ()>(addr)();
#endif
}

ffmpeg_initor::~ffmpeg_initor()
{
	avformat_network_deinit();
}

ffmpeg_initor initor;

}
