
#pragma once

#include <stdexcept>

#include "libencoder_api.hpp"

namespace libencoder
{

struct RECT
{
	RECT(){ top = bottom = left = right = 0;}

	RECT(int top_, int bottom_, int left_, int right_)
		: top(top_)
		, bottom(bottom_)
		, left(left_)
		, right(right_)
	{}

	int top;
	int bottom;
	int left;
	int right;
};

class CEncoder
{
public:
	CEncoder(const std::string& filename, RECT do_clip, int fps, int video_width, int video_height, bool keep_ratio, int video_quality, int samplerate)
		: m_filename(filename)
		, m_encoder(create_encoder(filename.c_str(), 2, samplerate, fps, video_width, video_height, keep_ratio, do_clip.top, do_clip.bottom, do_clip.left, do_clip.right))
		, m_is_capturing(false)
	{
		if(!m_encoder)
			throw std::bad_alloc();
		m_is_capturing = true;
	}

	CEncoder(const char* filename, int fps = 15, int video_quality = 5)
		: m_filename(filename)
		, m_encoder(create_encoder(filename, 2, 48000, 15, 1280, 720, true, 0, 0, 0, 0))
		, m_is_capturing(false)
	{
		if (!m_encoder)
			throw std::bad_alloc();
		m_is_capturing = true;
	}

	~CEncoder()
	{
		clean_up();
	}

	std::string get_filename() const
	{
		return m_filename;
	}

public:

	// 向视频编码器输入一帧视频.
	void feed_video_frame(uint8_t* data, int width, int height, int line_size, int64_t timestamp, bool flip_picture = false)
	{
		encoder_feed_video_frame(m_encoder, data, width, height, line_size, timestamp, flip_picture);
	}

	// 向音频编码器输入一帧音频.
	void feed_audio_frame(uint8_t* data, long size, int64_t timestamp)
	{
		encoder_feed_audio(m_encoder, data, size, timestamp);
	}
	void flush_encoded()
	{
		encoder_flush_frames(m_encoder);
	}

private:
	void clean_up()
	{
		//if(m_is_capturing)
			//encoder_stop_capture(m_encoder);
		destory_encoder(m_encoder);
	}

private:
	std::string m_filename;
	encoder_t* m_encoder;
	bool m_is_capturing;
};

}
