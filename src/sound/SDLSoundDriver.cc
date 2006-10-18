// $Id$

#include "SDLSoundDriver.hh"
#include "Mixer.hh"
#include "MSXException.hh"
#include "Math.hh"
#include "build-info.hh"
#include <algorithm>
#include <cassert>
#include <SDL.h>

namespace openmsx {

SDLSoundDriver::SDLSoundDriver(Mixer& mixer_,
                               unsigned wantedFreq, unsigned wantedSamples)
	: mixer(mixer_)
	, muted(true)
{
	SDL_AudioSpec desired;
	desired.freq     = wantedFreq;
	desired.samples  = Math::powerOfTwo(wantedSamples);
	desired.channels = 2; // stereo
	desired.format   = OPENMSX_BIGENDIAN ? AUDIO_S16MSB : AUDIO_S16LSB;
	desired.callback = audioCallbackHelper; // must be a static method
	desired.userdata = this;

	if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
		throw MSXException("Unable to initialize SDL audio subsystem: " +
		                   std::string(SDL_GetError()));
	}
	SDL_AudioSpec audioSpec;
	if (SDL_OpenAudio(&desired, &audioSpec) != 0) {
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
		throw MSXException("Unable to open SDL audio: " +
		                   std::string(SDL_GetError()));
	}
	frequency = audioSpec.freq;
	bufferSize = 4 * (audioSpec.size / sizeof(short));
	assert(Math::powerOfTwo(bufferSize) == bufferSize);
	bufferMask = bufferSize - 1;

	mixBuffer = new short[bufferSize];
	memset(mixBuffer, 0, bufferSize * sizeof(short));
	readIdx = writeIdx = 0;
}

SDLSoundDriver::~SDLSoundDriver()
{
	delete[] mixBuffer;

	SDL_CloseAudio();
	SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

void SDLSoundDriver::lock()
{
	SDL_LockAudio();
}

void SDLSoundDriver::unlock()
{
	SDL_UnlockAudio();
}

void SDLSoundDriver::mute()
{
	if (!muted) {
		muted = true;
		SDL_PauseAudio(1);
	}
}

void SDLSoundDriver::unmute()
{
	if (muted) {
		muted = false;
		readIdx = writeIdx = 0;
		SDL_PauseAudio(0);
	}
}

unsigned SDLSoundDriver::getFrequency() const
{
	return frequency;
}

unsigned SDLSoundDriver::getSamples() const
{
	return bufferSize / (4 * 2); // 4 * (left + right)
}

void SDLSoundDriver::audioCallbackHelper(void* userdata, byte* strm, int len)
{
	assert((len & 3) == 0); // stereo, 16-bit
	((SDLSoundDriver*)userdata)->
		audioCallback((short*)strm, len / sizeof(short));
}

void SDLSoundDriver::audioCallback(short* stream, unsigned len)
{
	assert((len & 1) == 0);
	available = (writeIdx - readIdx) & bufferMask;
	unsigned num = std::min<unsigned>(len, available);
	if ((readIdx + num) < bufferSize) {
		memcpy(stream, &mixBuffer[readIdx], num * sizeof(short));
		readIdx += num;
	} else {
		unsigned len1 = bufferSize - readIdx;
		memcpy(stream, &mixBuffer[readIdx], len1 * sizeof(short));
		unsigned len2 = num - len1;
		memcpy(&stream[len1], mixBuffer, len2 * sizeof(short));
		readIdx = len2;
	}
	int missing = len - available;
	if (missing > 0) {
		// buffer underrun
		mixer.bufferUnderRun(&stream[available], missing / 2);
	}
}

double SDLSoundDriver::uploadBuffer(short* buffer, unsigned len)
{
	len *= 2; // stereo
	unsigned free = (readIdx - writeIdx - 2) & bufferMask;
	unsigned num = std::min(len, free); // ignore overrun (drop samples)
	if ((writeIdx + num) < bufferSize) {
		memcpy(&mixBuffer[writeIdx], buffer, num * sizeof(short));
		writeIdx += num;
	} else {
		unsigned len1 = bufferSize - writeIdx;
		memcpy(&mixBuffer[writeIdx], buffer, len1 * sizeof(short));
		unsigned len2 = num - len1;
		memcpy(mixBuffer, &buffer[len1], len2 * sizeof(short));
		writeIdx = len2;
	}

	int target = bufferSize / 2;
	double factor = static_cast<double>(available) / target;
	available = target;
	return factor;
}

} // namespace openmsx
