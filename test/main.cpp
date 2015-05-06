
#include <libencoder.hpp>
#include <boost/thread.hpp>

int main(int argc, char **argv)
{
	
	libencoder::CEncoder encoder("test.mp4");

	boost::this_thread::sleep_for(boost::chrono::seconds(20));

//  portaudio::Stream capture_stream;

	return 0;
}
