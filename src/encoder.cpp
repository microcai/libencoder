
#include <boost/bind.hpp>
#include <boost/thread.hpp>
#include <boost/make_shared.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/regex.hpp>
#include <boost/lexical_cast.hpp>

#include <iostream>
#include <vector>
#include <bitset>
#include <array>
#include <string>

#if defined(_M_IX86) || defined(_M_X64)
#include <intrin.h>
#endif

#if defined(__GNUC__) && ( defined(__x86_64__) || defined(__i386__) )
#include <x86intrin.h>
#include <cpuid.h>
#endif

#include "encoder.hpp"
#include "ffmpeg_encoder.hpp"

static std::string calculated_preset = "fast";

namespace libencoder
{
	encoder::encoder(const char* filename, int audio_channel, int audio_sample_rate, int fps, int video_width, int video_height, bool keep_ratio, const rect& clip_rect_)
		: m_work(new boost::asio::io_service::work(m_io_service))
		, m_io_service_thread(boost::thread(boost::bind(&boost::asio::io_service::run, &m_io_service)))
		, clip_rect(clip_rect_)
		, m_keep_ratio(keep_ratio)
	{
		// extract type from extension
		std::string extension = boost::filesystem::path(filename).extension().string();
		if (extension.empty())
			extension = "ts";
		m_livecodec.reset(new ffmpeg_encoder(std::string(filename), extension.substr(1), std::string("9.0")));

		m_ac.channels = 2;
		m_ac.bit_rate = 64000;
		m_ac.bytes_persample = 2;
		m_ac.sample_rate = audio_sample_rate;
		m_livecodec->init_audio_encoder(m_ac);

		m_vc.fps = fps;
		m_vc.bit_rate = 1000;
		m_vc.height = video_height;
		m_vc.width = video_width;
		m_vc.preset = calculated_preset;
		m_vc.profile = "main";
		m_vc.fps_num = 1;
		m_vc.fps_den = m_vc.fps;

		m_livecodec->init_video_encoder(m_vc);

		m_livecodec->write_header();
	}

	encoder::~encoder()
	{
		m_work.reset();
		m_io_service.stop();
		m_io_service_thread.join();
	}

	void encoder::do_video_frame(uint8_t* data, int width, int height, int linesize, int64_t timestamp, bool flip_picture/* = false*/)
	{
		AVFrame* frame = av_frame_alloc();

		if (!flip_picture)
		{
			if (clip_rect.left == 0 && clip_rect.top == 0 && clip_rect.width() == width, clip_rect.height() == height)
			{
				memset(&clip_rect, 0, sizeof clip_rect);
			}
		}

		if (clip_rect.is_valid())
		{
			//			memset(&clip_buffer[0], 0, clip_buffer.size());
			int dst_copy_x = 0;
			int dst_copy_y = 0;

			int dst_real_width = clip_rect.width();
			int dst_real_height = clip_rect.height();

			if (m_keep_ratio)
			{
				// 检测是在 上下加黑边，还是在左右加黑边。
				bool margin_topdown = false;

				double r = (double)clip_rect.width() / (double)clip_rect.height();
				double fin_r = (double)m_vc.width / (double)m_vc.height;

				if (r >= fin_r)
				{
					margin_topdown = true;
				}

				if (margin_topdown)
				{
					// 上下加黑边，计算需要加几行黑边。
					dst_real_height = clip_rect.width() / fin_r;

					auto added_height = (dst_real_height - clip_rect.height());
					assert(added_height >= 0);
					if (added_height < 0)
						added_height = 0;

					// 将上下黑边平均分配到视频中.
					dst_copy_y = added_height / 2;
				}
				else
				{
					//  左右加黑边，计算需要加多少黑边
					dst_real_width = clip_rect.height() * fin_r;
					auto added_width = (dst_real_width - clip_rect.width());

					assert(added_width >= 0);
					if (added_width < 0)
						added_width = 0;
					// 将上下黑边平均分配到视频中.
					dst_copy_x = added_width / 2;
				}
			}

			clip_buffer.resize((dst_real_width + 8)*(dst_real_height + 8) * 4);

			avpicture_fill((AVPicture*)frame, clip_buffer.data(), AV_PIX_FMT_BGR0,
				dst_real_width, dst_real_height);

			auto stride = linesize;
			auto dst_stride = frame->linesize[0];

			auto copy_line_size = clip_rect.width() * 4;

			// 然后将视频从原始的 buffer  里拷贝到 clip_buffer.
			// 不拷贝覆盖的地方是 0 , 于是就黑边了.
			if (flip_picture)
			{
				for (int copy_Y = dst_copy_y, i_Y = 0; i_Y < clip_rect.height(); ++i_Y, ++copy_Y)
				{
					memcpy(frame->data[0] + dst_stride * copy_Y + dst_copy_x * 4,
						data + (height- 1 -  (clip_rect.top + i_Y)) * stride + clip_rect.left * 4,
						copy_line_size);
				}
			}
			else
			{
				for (int copy_Y = dst_copy_y, i_Y = 0; i_Y < clip_rect.height(); ++i_Y, ++copy_Y)
				{
					memcpy(frame->data[0] + dst_stride * copy_Y + dst_copy_x * 4,
						data + (clip_rect.top + i_Y) * stride + clip_rect.left * 4,
						copy_line_size);
				}
			}

			width = dst_real_width;
			height = dst_real_height;
		}
		else
		{
			avpicture_fill((AVPicture*)frame, data, AV_PIX_FMT_BGR0, width, height);
		}
		AVFrame* dst = av_frame_alloc();
		avpicture_fill((AVPicture*)dst, m_sws_buffer, AV_PIX_FMT_YUV420P, m_vc.width, m_vc.height);
		dst->width = m_vc.width;
		dst->height = m_vc.height;

		auto m_swsctx = sws_getContext(width, height, AV_PIX_FMT_BGR0, dst->width, dst->height,
			AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);
		auto ret = sws_scale(m_swsctx, frame->data, frame->linesize, 0, height, dst->data, dst->linesize);
		m_livecodec->do_video_frame(m_sws_buffer, dst->width, dst->height, timestamp);
		sws_freeContext(m_swsctx);
		av_frame_free(&frame);

		av_frame_free(&dst);
	}

	void encoder::do_audio_frame(uint8_t* data, long size, int64_t timestamp)
	{
		m_livecodec->do_audio_frame(data, size, timestamp);
	}

	void encoder::flush_and_write_tailer()
	{
		m_livecodec->flush_and_write_tailer();
	}
}

#ifdef _WIN32
float ProcSpeedCalc()
{

	// variables for the clock-cycles:
	__int64 cyclesStart = 0, cyclesStop = 0;
	// variables for the High-Res Preformance Counter:
	unsigned __int64 nCtr = 0, nFreq = 0, nCtrStop = 0;


	// retrieve performance-counter frequency per second:
	if (!QueryPerformanceFrequency((LARGE_INTEGER *)&nFreq)) return 0;

	// retrieve the current value of the performance counter:
	QueryPerformanceCounter((LARGE_INTEGER *)&nCtrStop);

	// add the frequency to the counter-value:
	nCtrStop += nFreq;

	cyclesStart = __rdtsc();

	do {
		// retrieve the value of the performance counter
		// until 1 sec has gone by:
		QueryPerformanceCounter((LARGE_INTEGER *)&nCtr);
	} while (nCtr < nCtrStop);

	cyclesStop = __rdtsc();

	// stop-start is speed in Hz divided by 1,000,000 is speed in MHz
	return    ((float)cyclesStop - (float)cyclesStart) / 1000000;
}

#elif __linux__

double ProcSpeedCalc()
{
	std::ifstream cpuinfo("/proc/cpuinfo");

	std::string line;

	do{

		line.resize(1000);

		cpuinfo.getline(&line[0], line.capacity());
		line.resize(strlen(line.data()));

		boost::smatch result;


		if (boost::regex_search(line, result, boost::regex("bogomips[ \t]+:[ \t]+([0-9\\.]+)")))
		{
			std::string s1 = result[1];
			return boost::lexical_cast<double>(s1);
		}
	}while(!cpuinfo.eof());

	return 1000.0;
}

#else
double ProcSpeedCalc()
{
	return 1000.0;
}
#endif


#ifdef _WIN32
class InstructionSet
{
	// forward declarations
	class InstructionSet_Internal;

public:
	// getters
	static std::string Vendor(void) { return CPU_Rep.vendor_; }
	static std::string Brand(void) { return CPU_Rep.brand_; }

	static bool SSE3(void) { return CPU_Rep.f_1_ECX_[0]; }
	static bool PCLMULQDQ(void) { return CPU_Rep.f_1_ECX_[1]; }
	static bool MONITOR(void) { return CPU_Rep.f_1_ECX_[3]; }
	static bool SSSE3(void) { return CPU_Rep.f_1_ECX_[9]; }
	static bool FMA(void) { return CPU_Rep.f_1_ECX_[12]; }
	static bool CMPXCHG16B(void) { return CPU_Rep.f_1_ECX_[13]; }
	static bool SSE41(void) { return CPU_Rep.f_1_ECX_[19]; }
	static bool SSE42(void) { return CPU_Rep.f_1_ECX_[20]; }
	static bool MOVBE(void) { return CPU_Rep.f_1_ECX_[22]; }
	static bool POPCNT(void) { return CPU_Rep.f_1_ECX_[23]; }
	static bool AES(void) { return CPU_Rep.f_1_ECX_[25]; }
	static bool XSAVE(void) { return CPU_Rep.f_1_ECX_[26]; }
	static bool OSXSAVE(void) { return CPU_Rep.f_1_ECX_[27]; }
	static bool AVX(void) { return CPU_Rep.f_1_ECX_[28]; }
	static bool F16C(void) { return CPU_Rep.f_1_ECX_[29]; }
	static bool RDRAND(void) { return CPU_Rep.f_1_ECX_[30]; }

	static bool MSR(void) { return CPU_Rep.f_1_EDX_[5]; }
	static bool CX8(void) { return CPU_Rep.f_1_EDX_[8]; }
	static bool SEP(void) { return CPU_Rep.f_1_EDX_[11]; }
	static bool CMOV(void) { return CPU_Rep.f_1_EDX_[15]; }
	static bool CLFSH(void) { return CPU_Rep.f_1_EDX_[19]; }
	static bool MMX(void) { return CPU_Rep.f_1_EDX_[23]; }
	static bool FXSR(void) { return CPU_Rep.f_1_EDX_[24]; }
	static bool SSE(void) { return CPU_Rep.f_1_EDX_[25]; }
	static bool SSE2(void) { return CPU_Rep.f_1_EDX_[26]; }

	static bool FSGSBASE(void) { return CPU_Rep.f_7_EBX_[0]; }
	static bool BMI1(void) { return CPU_Rep.f_7_EBX_[3]; }
	static bool HLE(void) { return CPU_Rep.isIntel_ && CPU_Rep.f_7_EBX_[4]; }
	static bool AVX2(void) { return CPU_Rep.f_7_EBX_[5]; }
	static bool BMI2(void) { return CPU_Rep.f_7_EBX_[8]; }
	static bool ERMS(void) { return CPU_Rep.f_7_EBX_[9]; }
	static bool INVPCID(void) { return CPU_Rep.f_7_EBX_[10]; }
	static bool RTM(void) { return CPU_Rep.isIntel_ && CPU_Rep.f_7_EBX_[11]; }
	static bool AVX512F(void) { return CPU_Rep.f_7_EBX_[16]; }
	static bool RDSEED(void) { return CPU_Rep.f_7_EBX_[18]; }
	static bool ADX(void) { return CPU_Rep.f_7_EBX_[19]; }
	static bool AVX512PF(void) { return CPU_Rep.f_7_EBX_[26]; }
	static bool AVX512ER(void) { return CPU_Rep.f_7_EBX_[27]; }
	static bool AVX512CD(void) { return CPU_Rep.f_7_EBX_[28]; }
	static bool SHA(void) { return CPU_Rep.f_7_EBX_[29]; }

	static bool PREFETCHWT1(void) { return CPU_Rep.f_7_ECX_[0]; }

	static bool LAHF(void) { return CPU_Rep.f_81_ECX_[0]; }
	static bool LZCNT(void) { return CPU_Rep.isIntel_ && CPU_Rep.f_81_ECX_[5]; }
	static bool ABM(void) { return CPU_Rep.isAMD_ && CPU_Rep.f_81_ECX_[5]; }
	static bool SSE4a(void) { return CPU_Rep.isAMD_ && CPU_Rep.f_81_ECX_[6]; }
	static bool XOP(void) { return CPU_Rep.isAMD_ && CPU_Rep.f_81_ECX_[11]; }
	static bool TBM(void) { return CPU_Rep.isAMD_ && CPU_Rep.f_81_ECX_[21]; }

	static bool SYSCALL(void) { return CPU_Rep.isIntel_ && CPU_Rep.f_81_EDX_[11]; }
	static bool MMXEXT(void) { return CPU_Rep.isAMD_ && CPU_Rep.f_81_EDX_[22]; }
	static bool RDTSCP(void) { return CPU_Rep.isIntel_ && CPU_Rep.f_81_EDX_[27]; }
	static bool _3DNOWEXT(void) { return CPU_Rep.isAMD_ && CPU_Rep.f_81_EDX_[30]; }
	static bool _3DNOW(void) { return CPU_Rep.isAMD_ && CPU_Rep.f_81_EDX_[31]; }

private:
	static const InstructionSet_Internal CPU_Rep;

	class InstructionSet_Internal
	{
	public:
		InstructionSet_Internal()
			: nIds_{ 0 },
			nExIds_{ 0 },
			isIntel_{ false },
			isAMD_{ false },
			f_1_ECX_{ 0 },
			f_1_EDX_{ 0 },
			f_7_EBX_{ 0 },
			f_7_ECX_{ 0 },
			f_81_ECX_{ 0 },
			f_81_EDX_{ 0 },
			data_{},
			extdata_{}
		{
			//int cpuInfo[4] = {-1};
			std::array<int, 4> cpui;

			// Calling __cpuid with 0x0 as the function_id argument
			// gets the number of the highest valid function ID.
#ifdef __GNUC__

			__cpuid(0, cpui[0], cpui[1], cpui[2], cpui[3]);
#else
			__cpuid(cpui.data(), 0);
#endif
			nIds_ = cpui[0];

			for (int i = 0; i <= nIds_; ++i)
			{
#ifdef __GNUC__
				__cpuid_count(0, i, cpui[0], cpui[1], cpui[2], cpui[3]);
#else
				__cpuidex(cpui.data(), i, 0);
#endif
				data_.push_back(cpui);
			}

			// Capture vendor string
			char vendor[0x20];
			memset(vendor, 0, sizeof(vendor));
			*reinterpret_cast<int*>(vendor) = data_[0][1];
			*reinterpret_cast<int*>(vendor + 4) = data_[0][3];
			*reinterpret_cast<int*>(vendor + 8) = data_[0][2];
			vendor_ = vendor;
			if (vendor_ == "GenuineIntel")
			{
				isIntel_ = true;
			}
			else if (vendor_ == "AuthenticAMD")
			{
				isAMD_ = true;
			}

			// load bitset with flags for function 0x00000001
			if (nIds_ >= 1)
			{
				f_1_ECX_ = data_[1][2];
				f_1_EDX_ = data_[1][3];
			}

			// load bitset with flags for function 0x00000007
			if (nIds_ >= 7)
			{
				f_7_EBX_ = data_[7][1];
				f_7_ECX_ = data_[7][2];
			}

			// Calling __cpuid with 0x80000000 as the function_id argument
			// gets the number of the highest valid extended ID.
#ifdef __GNUC__

			__cpuid(0x80000000, cpui[0], cpui[1], cpui[2], cpui[3]);
#else
			__cpuid(cpui.data(), 0x80000000);
#endif
			nExIds_ = cpui[0];

			char brand[0x40];
			memset(brand, 0, sizeof(brand));

			for (int i = 0x80000000; i <= nExIds_; ++i)
			{
#ifdef __GNUC__

			__cpuid(0x80000000, cpui[0], cpui[1], cpui[2], cpui[3]);
#else
			__cpuid(cpui.data(), 0x80000000);
#endif
				extdata_.push_back(cpui);
			}

			// load bitset with flags for function 0x80000001
			if (nExIds_ >= 0x80000001)
			{
				f_81_ECX_ = extdata_[1][2];
				f_81_EDX_ = extdata_[1][3];
			}

			// Interpret CPU brand string if reported
			if (nExIds_ >= 0x80000004)
			{
				memcpy(brand, extdata_[2].data(), sizeof(cpui));
				memcpy(brand + 16, extdata_[3].data(), sizeof(cpui));
				memcpy(brand + 32, extdata_[4].data(), sizeof(cpui));
				brand_ = brand;
			}
		};

		int nIds_;
		int nExIds_;
		std::string vendor_;
		std::string brand_;
		bool isIntel_;
		bool isAMD_;
		std::bitset<32> f_1_ECX_;
		std::bitset<32> f_1_EDX_;
		std::bitset<32> f_7_EBX_;
		std::bitset<32> f_7_ECX_;
		std::bitset<32> f_81_ECX_;
		std::bitset<32> f_81_EDX_;
		std::vector<std::array<int, 4>> data_;
		std::vector<std::array<int, 4>> extdata_;
	};
};

// Initialize static member data
const InstructionSet::InstructionSet_Internal InstructionSet::CPU_Rep;

static InstructionSet set;

#endif

#if defined(_M_IX86) || defined(_M_X64) || ( defined(__GNUC__) && ( defined(__x86_64__) || defined(__i386__) ) )


#ifdef __GNUC__
bool cpu_has_aes()
{
	std::bitset<32> f_1_ECX_;

	std::array<unsigned, 4> cpui;

	__get_cpuid(1, &cpui[0], &cpui[1], &cpui[2], &cpui[3]);

	f_1_ECX_ = cpui[2];

	return f_1_ECX_[25];
}
#endif

extern "C"
{
	ENCODER_API void encoder_do_benchmark_and_setup_parameters()
	{
		auto freq = ProcSpeedCalc();

#ifdef _WIN32
		if (set.AVX())
			freq *= 1.1;
		if (set.AVX2())
			freq *= 1.05;
		if (set.SSE42())
			freq *= 1.1;
		if (set._3DNOW())
			freq *= 1.2;
#endif
		// 可选参数为 fast / ultrafst 两个.

		// i3 以上 cpu 为 fast
		if (boost::thread::hardware_concurrency() == 4)
		{
			// 4 核，那么 fast.
			// 如果是 i3 ... 额，那还是不能 *4 只能 *2.5
			// i3 是木有 AES的。

#ifdef _WIN32
			if (set.AES())
#else
			if (cpu_has_aes())
#endif
				freq *= 4;
			else
				freq *= 2.5;
		}

		if (boost::thread::hardware_concurrency() >= 6)
		{
			// 4 核超线能提供 6 倍单核的能力.
			freq *= 6;
		}

		if (boost::thread::hardware_concurrency() < 4)
		{
			freq *= boost::thread::hardware_concurrency();
		}

		if (freq > 42000)
		{
			calculated_preset = "placebo";
		}
		else if (freq > 33000)
		{
			calculated_preset = "veryslow";
		}
		else if (freq > 20000)
		{
			calculated_preset = "slower";
		}
		else if (freq > 12000)
		{
			calculated_preset = "slow";
		}
		else if (freq > 6500)
		{
			calculated_preset = "medium";
		}
		else if (freq < 5500)
		{
			calculated_preset = "ultrafast";
		}
		else if (freq < 10500)
		{
			calculated_preset = "veryfast";
		}
	}
}

#else
extern "C"
{
ENCODER_API void encoder_do_benchmark_and_setup_parameters(){}
}
#endif
