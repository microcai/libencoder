
#pragma once

#include <boost/scoped_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/asio.hpp>

#include <libencoder_api.hpp>
#include "ffmpeg_encoder.hpp"

namespace libencoder{

struct rect {
	int top, bottom, left, right;

	rect() { top = bottom = left = right; }

	bool is_valid()
	{
		return !(top == bottom && left == right);
	}

	int width() const { return right - left; }
	int height() const { return bottom - top; }
};


class ffmpeg_encoder;
class encoder
{
public:
	encoder(const char* filename, int audio_channel, int audio_sample_rate, int fps, int video_width, int video_height, bool keep_ratio, const rect& clip_rect);
	~encoder();

public:
	// 向视频编码器输入一帧视频.
	void do_video_frame(uint8_t* data, int width, int height, int linesize, int64_t timestamp, bool flip_picture = false);

	// 向音频编码器输入一帧音频.
	void do_audio_frame(uint8_t* data, long size, int64_t timestamp);

	void flush_and_write_tailer();

private:
	boost::asio::io_service m_io_service;
	boost::scoped_ptr<boost::asio::io_service::work> m_work;
	boost::thread m_io_service_thread;

	int m_audio_channel;
	int audio_sample_rate;
	int video_width;
	int video_height;

	rect clip_rect;
	std::vector<uint8_t> clip_buffer;

	uint8_t m_sws_buffer[1280 * 720 * 2];
	boost::shared_ptr<ffmpeg_encoder> m_livecodec;
	audio_config m_ac;
	video_config m_vc;

	int _clip_top; // 如果剪切，这个是视频的上边界.
	int _clip_height; // 如果剪切，这个是视频的高度.
	bool m_keep_ratio;
};


}
