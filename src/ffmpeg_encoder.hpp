
#pragma once
#pragma warning(disable: 4097 4511 4512 4514 4705 4244 4996)

#include <stdint.h>
#include <cstdint>
#include <string>
#include <vector>
#include <list>
#include <fstream>
#include <string>
#include <map>

#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/thread.hpp>
#include <boost/noncopyable.hpp>
#include <boost/unordered_map.hpp>
#include <boost/asio.hpp>
#include <boost/assert.hpp>

extern "C"
{
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavfilter/avfilter.h"
#include "libavfilter/buffersrc.h"
#include "libavfilter/buffersink.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
#include "libavutil/time.h"
#include "libavutil/intreadwrite.h"
#include "libavutil/audio_fifo.h"
#include "libswresample/swresample.h"
#include "libavutil/opt.h"
}

namespace libencoder{

struct video_config
{
	std::string profile;
	std::string preset;
	std::string tune;
	int width;
	int height;
	float fps;
	int threads;
	int fps_num; ///< numerator
	int fps_den; ///< denominator
	int bit_rate;
};

struct audio_config
{
	int sample_rate;
	int channels;
	int bytes_persample;
	int bit_rate;
};

enum {
	stat_seconds = 30
};

struct codec_stat
{
	// 音频码率计算.
	int64_t a_total_bytes;
	int64_t a_run_time;
	int a_bit_rate;

	// 音频码率计算.
	int64_t v_total_bytes;
	int64_t v_run_time;
	int v_bit_rate;

	// 输出码率计算.
	int64_t total_bytes;
	int bit_rate;
	int64_t run_time;
	int64_t total_run_time;
	int run_time_log;
};

class ffmpeg_encoder : public boost::noncopyable
{
public:
	ffmpeg_encoder(const std::string& live_name, std::string fmt = "mp4", std::string version = "");
	~ffmpeg_encoder();

public:
	// 初始化视频编码器, 默认为libx264编码器.
	void init_video_encoder(video_config vc, std::string encoder = "libx264");

	// 向视频编码器输入一帧视频.
	void do_video_frame(uint8_t* data, int width, int height, int64_t timestamp);

	// 初始化音频编码器, 默认为 libvo_aacenc 编码器.
	void init_audio_encoder(audio_config ac, std::string encoder = "libvo_aacenc");

	// 向音频编码器输入一帧音频.
	void do_audio_frame(uint8_t* data, long size, int64_t timestamp);

	// 在初始化音频和视频编码器后, 必须调用write_header来写入视频格式头.
	void write_header();

	// 把delay的编码写入掉.
	void flush();
	// 关闭文件的时候调用这个.
	void flush_and_write_tailer();

	// 音频音量调节.
	void volume(int vol);
private:
	void audio_volume(uint8_t* buffer, int size, int vol);
	void SwrConvert(uint8_t* buffer, int size, AVFrame** dst);

private:
	AVFormatContext* m_fmt_ctx;
	std::string m_fmt_name;
	AVCodecContext* m_h264_ctx;
	AVCodecContext* m_audio_ctx;
	AVStream* m_video_stream;
	AVStream* m_audio_stream;
	boost::asio::streambuf m_streambuf;
	int64_t m_vframe_index;
	int64_t m_aframe_index;
	SwrContext* m_swr_ctx;
	std::vector<uint8_t> m_sws_buffer;
	int m_sws_buffer_size;
	struct SwsContext* m_swsctx;
	std::vector<uint8_t> m_swr_buffer;
	std::vector<uint8_t> m_audio_buffer;

	boost::mutex m_mutex;
	uint8_t* m_clone_frame;
	int m_clone_frame_len;

	std::string m_live_name;
	int m_volume;
	bool m_head_video;
	bool m_head_audio;
	bool m_head_meta;
};

}
