
#include "ffmpeg_encoder.hpp"

#define IO_BUFFER_SIZE	32768

namespace libencoder {

ffmpeg_encoder::ffmpeg_encoder(const std::string& live_name, std::string fmt /*= "mpegts"*/, std::string version/* = ""*/)
	: m_fmt_ctx(NULL)
	, m_h264_ctx(NULL)
	, m_audio_ctx(NULL)
	, m_swr_ctx(NULL)
	, m_vframe_index(1)
	, m_aframe_index(0)
	, m_swsctx(NULL)
	, m_clone_frame_len(0)
	, m_clone_frame(NULL)
	, m_volume(256)
	, m_live_name(live_name)
{
	m_fmt_name = fmt;
	m_fmt_ctx = avformat_alloc_context();
	m_fmt_ctx->oformat = av_guess_format(fmt.c_str(), NULL, NULL);
	if (!m_fmt_ctx->oformat)
	{
		m_fmt_name = "mpegts";
		m_fmt_ctx->oformat = av_guess_format("mpegts", NULL, NULL);
	}
	if (!m_fmt_ctx->oformat)
	{
		throw std::runtime_error("Could not guess format: ");
	}

	unsigned char* io_buffer = reinterpret_cast<unsigned char*>(av_malloc(IO_BUFFER_SIZE));

	avio_open2(&m_fmt_ctx->pb, live_name.c_str(), AVIO_FLAG_READ_WRITE, NULL, NULL);

	//if (fmt != "flv")
		//m_fmt_ctx->pb->direct = 1;

	av_dict_free(&m_fmt_ctx->metadata);
	std::string name = "libencoder-" + version;
	if (!version.empty())
		name = "libencoder-" + version;
	av_dict_set(&m_fmt_ctx->metadata, "service_name", name.c_str(), 0);
	av_dict_set(&m_fmt_ctx->metadata, "title", "", 0);
	av_dict_set(&m_fmt_ctx->metadata, "service_provider", "wanin.net", 0);
}

ffmpeg_encoder::~ffmpeg_encoder()
{
	if (m_swr_ctx)
		swr_free(&m_swr_ctx);
	if (m_clone_frame)
		av_free(m_clone_frame);
	if (m_swsctx)
		sws_freeContext(m_swsctx);
	if (m_h264_ctx)
		avcodec_close(m_h264_ctx);
	if (m_audio_ctx)
		avcodec_close(m_audio_ctx);
	avformat_free_context(m_fmt_ctx);
}

void ffmpeg_encoder::init_video_encoder(video_config vc, std::string encoder /*= "libx264"*/)
{
	AVCodec* codec = nullptr;
	AVCodec* codec_hwaccel = avcodec_find_encoder_by_name("h264_qsv");

	if (!codec_hwaccel)
	{
		codec = avcodec_find_encoder_by_name(encoder.c_str());
		if (!codec)
			codec = avcodec_find_encoder(AV_CODEC_ID_H264);
	}

	m_video_stream = avformat_new_stream(m_fmt_ctx, codec);
	if (!m_video_stream)
	{
		throw std::runtime_error("Could not allocate video stream!");
		return;
	}
	m_video_stream->id = 40;
	m_h264_ctx = m_video_stream->codec;
	m_h264_ctx->thread_count = boost::thread::hardware_concurrency();
	m_h264_ctx->thread_type = FF_THREAD_FRAME;
	if (!m_h264_ctx)
	{
		throw std::runtime_error("Could not allocate video codec context!");
		return;
	}

	if (m_fmt_name == "flv" || m_fmt_name == "mp4")
		m_h264_ctx->flags |= CODEC_FLAG_GLOBAL_HEADER;

	m_h264_ctx->bit_rate = vc.bit_rate * 1000;
	m_h264_ctx->rc_max_rate = vc.bit_rate * 2000;
	m_h264_ctx->rc_min_rate = 3000;
	// 	m_h264_ctx->rc_max_rate = m_h264_ctx->bit_rate;
	// 	m_h264_ctx->flags |= CODEC_FLAG_QSCALE;
	m_h264_ctx->rc_buffer_size = 30 * 1024 * 1024;
	m_h264_ctx->width = vc.width;
	m_h264_ctx->height = vc.height;
	// frames per second.
	AVRational rate = { 1, 10000 };
	m_h264_ctx->time_base = rate;// av_inv_q(rate);
	m_video_stream->time_base = m_h264_ctx->time_base;

	m_h264_ctx->ticks_per_frame;
	m_h264_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
	int profile = FF_PROFILE_H264_HIGH;
	if (!vc.profile.empty())
	{
		if (vc.profile == "baseline")
			profile = FF_PROFILE_H264_BASELINE;
		if (vc.profile == "main")
			profile = FF_PROFILE_H264_MAIN;
		if (vc.profile == "high")
			profile = FF_PROFILE_H264_HIGH;
		if (vc.profile == "high10")
			profile = FF_PROFILE_H264_HIGH_10;
		if (vc.profile == "high422")
			profile = FF_PROFILE_H264_HIGH_422;
		if (vc.profile == "high444")
			profile = FF_PROFILE_H264_HIGH_444;
	}
	m_h264_ctx->profile = profile;
	m_h264_ctx->thread_count = vc.threads;
	m_h264_ctx->thread_count = m_h264_ctx->thread_count > 16 ? 16 : m_h264_ctx->thread_count;
	m_h264_ctx->keyint_min = (vc.fps_num / vc.fps_den) / 2;
	m_h264_ctx->gop_size = vc.fps * 15;
	if (!vc.preset.empty())
		av_opt_set(m_h264_ctx->priv_data, "preset", vc.preset.c_str(), 0);

	m_h264_ctx->qmin = 18;
	m_h264_ctx->qmax = 25;

	if (!vc.tune.empty() && vc.tune != "film")
		av_opt_set(m_h264_ctx->priv_data, "tune", vc.tune.c_str(), 0);
	av_opt_set_int(m_h264_ctx->priv_data, "rc-lookahead", 100, 0);

	AVDictionary* encoder_opts = nullptr;
	av_dict_set(&encoder_opts, "threads", "auto", 0);

	m_video_stream->avg_frame_rate = { (int)vc.fps, 1 };
	if (avcodec_open2(m_h264_ctx, codec, &encoder_opts) < 0)
	{
		av_dict_free(&encoder_opts);
		throw std::runtime_error("Could not open h264 codec!");
		return;
	}
	av_dict_free(&encoder_opts);
	m_sws_buffer_size = avpicture_get_size(AV_PIX_FMT_YUV420P, vc.width, vc.height);
	m_sws_buffer.resize(m_sws_buffer_size);
}

void ffmpeg_encoder::do_video_frame(uint8_t* data, int width, int height, int64_t timestamp)
{
	AVFrame* frame = av_frame_alloc();
	frame->format = AV_PIX_FMT_YUV420P;
	int got_output;
	int ret;

	ret = avpicture_fill(reinterpret_cast<AVPicture*>(frame), data, AV_PIX_FMT_YUV420P, width, height);
	if (ret <= 0)
	{
		av_frame_free(&frame);
		return;
	}

	frame->format = AV_PIX_FMT_YUV420P;
	frame->width = width;
	frame->height = height;

	AVPacket pkt;
	av_init_packet(&pkt);
	pkt.data = NULL;
	pkt.size = 0;

	frame->pts =  timestamp / 100;// timestamp;
	m_vframe_index++;

	frame->width = m_h264_ctx->width;
	frame->height = m_h264_ctx->height;

	if (!m_h264_ctx)
	{
		av_frame_free(&frame);
		return;
	}

	ret = avcodec_encode_video2(m_h264_ctx, &pkt, frame, &got_output);
	if (ret != 0)
	{
		// LOG_ERR << "Video encoding failed!";
		av_frame_free(&frame);
		return;
	}
	if (got_output)
	{
		pkt.stream_index = m_video_stream->index;
		av_packet_rescale_ts(&pkt,m_h264_ctx->time_base, m_video_stream->time_base);

		boost::mutex::scoped_lock l(m_mutex);

		if(pkt.buf)
			ret = av_interleaved_write_frame(m_fmt_ctx, &pkt);
		if (ret < 0) {

		}
		av_free_packet(&pkt);
	}
	av_frame_free(&frame);
}

void ffmpeg_encoder::init_audio_encoder(audio_config ac, std::string encoder /*= "libvo_aacenc"*/)
{
	AVCodec* codec = avcodec_find_encoder_by_name(encoder.c_str());
	if (!codec)
		codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
	if (!codec)
	{
		throw std::runtime_error("Could not open audio codec!");
		return;
	}
	m_audio_stream = avformat_new_stream(m_fmt_ctx, codec);
	if (!m_audio_stream)
	{
		throw std::runtime_error("Could not allocate audio stream!");
		return;
	}
	m_audio_stream->id = 50;
	m_audio_ctx = m_audio_stream->codec;
	if (!m_audio_ctx)
	{
		throw std::runtime_error("Could not allocate audio codec context!");
		return;
	}

	if (m_fmt_name == "flv" || m_fmt_name == "mp4")
		m_audio_ctx->flags |= CODEC_FLAG_GLOBAL_HEADER;

	m_audio_ctx->bit_rate = ac.bit_rate * 1000;
	m_audio_ctx->channels = ac.channels;
	m_audio_ctx->sample_rate = ac.sample_rate;
	if (ac.bytes_persample == 2)
		m_audio_ctx->sample_fmt = AV_SAMPLE_FMT_S16P;	// maybe can use av_get_sample_fmt.
	if (ac.bytes_persample == 1)
		m_audio_ctx->sample_fmt = AV_SAMPLE_FMT_U8;		// maybe can use av_get_sample_fmt.
	if (m_audio_ctx->channels == 2)
		m_audio_ctx->channel_layout = AV_CH_LAYOUT_STEREO;
	if (codec->id == AV_CODEC_ID_AAC)
	{
		m_audio_ctx->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;
		m_audio_ctx->sample_fmt = AV_SAMPLE_FMT_FLTP;
		if (std::string(codec->name) == "libvo_aacenc")
			m_audio_ctx->sample_fmt = AV_SAMPLE_FMT_S16;
	}

	AVRational rate = { 1, (int)ac.sample_rate };
	// AVRational rate = { 1, 1000 };
	m_audio_stream->time_base = rate;//AVRational{ 1, ac.sample_rate };
	m_audio_ctx->time_base = rate;	// m_audio_ctx->channel_layout;
	auto ret = avcodec_open2(m_audio_ctx, codec, NULL);
}

void ffmpeg_encoder::do_audio_frame(uint8_t* data, long size, int64_t timestamp)
{
	if (!m_audio_ctx)
		return;

	AVFrame* frame = av_frame_alloc();
	int want_data_size = m_audio_ctx->frame_size * m_audio_ctx->channels * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
	if (want_data_size <= 0)
	{
		// LOG_WARN << "want_data_size == " << want_data_size;
		av_frame_free(&frame);
		return;
	}

	{
		boost::asio::streambuf::mutable_buffers_type bufs = m_streambuf.prepare(size);
		char* buf_ptr = boost::asio::buffer_cast<char*>(bufs);
		memcpy(buf_ptr, data, size);
		m_streambuf.commit(size);
	}

	m_audio_buffer.resize(want_data_size);

	int64_t time_unit = (m_audio_ctx->frame_size * 10000000LL) / m_audio_ctx->sample_rate;

	int ret = 0;
	do {
		if (want_data_size > static_cast<int>(m_streambuf.size()))
			break;

		ret = (int)m_streambuf.sgetn((char*) & m_audio_buffer[0], want_data_size);
		//if (ret != want_data_size) 	LOG_WARN << "sgetn, ret != want_data_size";

		if (m_volume != 256)
			audio_volume(&m_audio_buffer[0], want_data_size, m_volume);

		SwrConvert(&m_audio_buffer[0], want_data_size, &frame);

		int got_output;
		AVPacket pkt;
		av_init_packet(&pkt);
		pkt.data = NULL;
		pkt.size = 0;

		if (timestamp == -1)
		{
			frame->pts = m_aframe_index;
			AVRational ra;
			ra.num = 1;
			ra.den = m_audio_ctx->sample_rate;
			frame->pts = av_rescale_q(frame->pts, ra, m_audio_ctx->time_base);
			m_aframe_index += frame->nb_samples;
		}
		else
		{
			frame->pts = timestamp;
			timestamp += time_unit;
		}

		ret = avcodec_encode_audio2(m_audio_ctx, &pkt, frame, &got_output);
		if (ret != 0)
		{
			break;
		}
		if (got_output)
		{
			pkt.stream_index = m_audio_stream->index;
			av_packet_rescale_ts(&pkt, m_audio_ctx->time_base, m_audio_stream->time_base);

#if FF_API_CODED_FRAME
			if (m_audio_ctx && m_audio_ctx->coded_frame && m_audio_ctx->coded_frame->key_frame)
				pkt.flags |= AV_PKT_FLAG_KEY;
#endif
			boost::mutex::scoped_lock l(m_mutex);
			assert(pkt.buf);
			ret = av_interleaved_write_frame(m_fmt_ctx, &pkt);
			if (ret < 0)
			{
			}
			av_free_packet(&pkt);
		}
	} while (true);
	av_frame_free(&frame);
}

void ffmpeg_encoder::write_header()
{
	int ret = avformat_write_header(m_fmt_ctx, NULL/*&dict*/);
}

void ffmpeg_encoder::volume(int vol)
{
	m_volume = vol;
}

void ffmpeg_encoder::audio_volume(uint8_t* buffer, int size, int vol)
{
	if (vol <= 0) vol = 256;
	short* volp = (short *)buffer;
	for (int i = 0; i < size / m_audio_ctx->channels; i++)
	{
		int v = ((*volp) * vol + 128) >> 8;
		if (v < -32768) v = -32768;
		if (v > 32767) v = 32767;
		*volp++ = v;
	}
}

void ffmpeg_encoder::SwrConvert(uint8_t* buffer, int size, AVFrame** dst)
{
	int bytes = av_get_bytes_per_sample(m_audio_ctx->sample_fmt) * m_audio_ctx->channels * m_audio_ctx->frame_size;
	m_swr_buffer.resize(bytes);

	AVFrame* frame = *dst;
	frame->nb_samples = m_audio_ctx->frame_size;
	frame->format = m_audio_ctx->sample_fmt;
	frame->channel_layout = m_audio_ctx->channel_layout;

	frame->data[0] = & m_swr_buffer[0];
	frame->data[1] = frame->data[0] + (bytes / 2);
	uint8_t** out = frame->data;
	uint8_t** in = &buffer;

	if (!m_swr_ctx)
	{
		m_swr_ctx = swr_alloc_set_opts(NULL,
			AV_CH_LAYOUT_STEREO, m_audio_ctx->sample_fmt, m_audio_ctx->sample_rate,
			AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_S16, m_audio_ctx->sample_rate,
			0, NULL);

		 (swr_init(m_swr_ctx) < 0);
		//	LOG_ERR << "swr_init(<-) failed!";
	}

	if (m_swr_ctx)
	{
		int nb_samples = size / (m_audio_ctx->channels * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16));

		int out_linesize;
		int needed_buf_size = av_samples_get_buffer_size(&out_linesize,
			m_audio_ctx->channels,
			frame->nb_samples,
			AV_SAMPLE_FMT_S16, 1);

		int ret = swr_convert(m_swr_ctx, out, nb_samples, (const uint8_t**)in, nb_samples);
		if (ret == nb_samples)
		{
			ret = avcodec_fill_audio_frame(frame, m_audio_ctx->channels, m_audio_ctx->sample_fmt, &m_swr_buffer[0], bytes, 1);
			//if (ret < 0) LOG_ERR << "avcodec_fill_audio_frame(<-) failed!";
		}
	}
}

void ffmpeg_encoder::flush()
{
	auto want_data_size = m_audio_ctx->frame_size * m_audio_ctx->channels * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);

	// 声音视频分别flush.
	//boost::mutex::scoped_lock l(m_mutex);
	while (m_streambuf.size() > want_data_size)
	{
		do_audio_frame(0, 0, -1);
	}

	int got_output;

	do {
		AVPacket pkt;
		av_init_packet(&pkt);
		pkt.data = NULL;
		pkt.size = 0;

		auto ret = avcodec_encode_audio2(m_audio_ctx, &pkt, NULL, &got_output);
		if (ret != 0)
		{
			break;
		}
		if (got_output)
		{
			pkt.stream_index = m_audio_stream->index;
			av_packet_rescale_ts(&pkt, m_audio_ctx->time_base, m_audio_stream->time_base);

#if FF_API_CODED_FRAME
			if (m_audio_ctx && m_audio_ctx->coded_frame && m_audio_ctx->coded_frame->key_frame)
				pkt.flags |= AV_PKT_FLAG_KEY;
#endif
			boost::mutex::scoped_lock l(m_mutex);
			assert(pkt.buf);
			ret = av_interleaved_write_frame(m_fmt_ctx, &pkt);
			if (ret < 0)
			{
			}
			av_free_packet(&pkt);
		}
	} while (got_output);

	// 然后是视频.
	do {
		AVPacket pkt;
		av_init_packet(&pkt);
		pkt.data = NULL;
		pkt.size = 0;

		auto ret = avcodec_encode_video2(m_h264_ctx, &pkt, NULL, &got_output);
		if (ret != 0)
		{
			return;
		}
		if (got_output)
		{
			pkt.stream_index = m_video_stream->index;
			av_packet_rescale_ts(&pkt, m_h264_ctx->time_base, m_video_stream->time_base);

			boost::mutex::scoped_lock l(m_mutex);

			ret = av_interleaved_write_frame(m_fmt_ctx, &pkt);
			if (ret < 0)
			{
			}
			av_free_packet(&pkt);
		}
	}while(got_output);
}

void ffmpeg_encoder::flush_and_write_tailer()
{
	if (m_fmt_ctx->pb)
	{
		this->flush();
		{
			boost::mutex::scoped_lock l(m_mutex);
			AV_TIME_BASE;
			m_fmt_ctx->duration = AV_TIME_BASE * (m_aframe_index / (double)m_audio_ctx->sample_rate);
			m_video_stream->nb_frames;
			m_video_stream->duration = 10000 * (m_aframe_index / (double)m_audio_ctx->sample_rate);
			av_write_trailer(m_fmt_ctx);
		}
		avio_close(m_fmt_ctx->pb);
	}
}

}
