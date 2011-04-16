// $Id$

#include "LibAOSoundDriver.hh"
#include "MSXException.hh"

#include <ao/ao.h>
#include <cstring>


namespace openmsx {

static const int BITS_PER_SAMPLE = 16;
static const int CHANNELS = 2;

LibAOSoundDriver::LibAOSoundDriver(unsigned sampleRate_, unsigned bufferSize_)
	: sampleRate(sampleRate_)
	, bufferSize(bufferSize_)
{
	ao_initialize();

	int driver = ao_default_driver_id();

	ao_sample_format format;
	memset(&format, 0, sizeof(format));
	format.bits = BITS_PER_SAMPLE;
	format.channels = CHANNELS;
	format.rate = sampleRate;
	format.byte_format = AO_FMT_NATIVE;

	device = ao_open_live(driver, &format, NULL /* no options */);
	if (!device) {
		throw MSXException("Couldn't open audio device");
	}

	
}

LibAOSoundDriver::~LibAOSoundDriver()
{
	ao_close(device);
	ao_shutdown();
}

void LibAOSoundDriver::mute()
{
}

void LibAOSoundDriver::unmute()
{
}

unsigned LibAOSoundDriver::getFrequency() const
{
	return sampleRate;
}

unsigned LibAOSoundDriver::getSamples() const
{
	return bufferSize;
}

double LibAOSoundDriver::uploadBuffer(short* buffer, unsigned len)
{
	ao_play(
		device,
		reinterpret_cast<char*>(buffer),
		len * CHANNELS * (BITS_PER_SAMPLE / 8)
		);
	return 1.0;
}

} // namespace openmsx
