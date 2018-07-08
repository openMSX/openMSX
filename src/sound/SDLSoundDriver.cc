#include "SDLSoundDriver.hh"
#include "Reactor.hh"
#include "MSXMotherBoard.hh"
#include "RealTime.hh"
#include "GlobalSettings.hh"
#include "ThrottleManager.hh"
#include "MSXException.hh"
#include "Math.hh"
#include "Timer.hh"
#include "build-info.hh"
#include <SDL.h>
#include <algorithm>
#include <cassert>
#include <cstring>

namespace openmsx {

SDLSoundDriver::SDLSoundDriver(Reactor& reactor_,
                               unsigned wantedFreq, unsigned wantedSamples)
	: reactor(reactor_)
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
		throw MSXException(
			"Unable to initialize SDL audio subsystem: ",
			SDL_GetError());
	}
	SDL_AudioSpec audioSpec;
	if (SDL_OpenAudio(&desired, &audioSpec) != 0) {
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
		throw MSXException("Unable to open SDL audio: ", SDL_GetError());
	}

	frequency = audioSpec.freq;
	fragmentSize = audioSpec.samples;

	mixBufferSize = 3 * (audioSpec.size / sizeof(int16_t)) + 2;
	mixBuffer.resize(mixBufferSize);
	reInit();
}

SDLSoundDriver::~SDLSoundDriver()
{
	SDL_CloseAudio();
	SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

void SDLSoundDriver::reInit()
{
	SDL_LockAudio();
	readIdx  = 0;
	writeIdx = 0;
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
		reInit();
		SDL_PauseAudio(0);
	}
}

unsigned SDLSoundDriver::getFrequency() const
{
	return frequency;
}

unsigned SDLSoundDriver::getSamples() const
{
	return fragmentSize;
}

void SDLSoundDriver::audioCallbackHelper(void* userdata, byte* strm, int len)
{
	assert((len & 3) == 0); // stereo, 16-bit
	static_cast<SDLSoundDriver*>(userdata)->
		audioCallback(reinterpret_cast<int16_t*>(strm), len / sizeof(int16_t));
}

unsigned SDLSoundDriver::getBufferFilled() const
{
	int result = writeIdx - readIdx;
	if (result < 0) result += mixBufferSize;
	assert((0 <= result) && (unsigned(result) < mixBufferSize));
	return result;
}

unsigned SDLSoundDriver::getBufferFree() const
{
	// we can't distinguish completely filled from completely empty
	// (in both cases readIx would be equal to writeIdx), so instead
	// we define full as '(writeIdx + 2) == readIdx' (note that index
	// increases in steps of 2 (stereo)).
	int result = mixBufferSize - 2 - getBufferFilled();
	assert((0 <= result) && (unsigned(result) < mixBufferSize));
	return result;
}

void SDLSoundDriver::audioCallback(int16_t* stream, unsigned len)
{
	assert((len & 1) == 0); // stereo
	unsigned available = getBufferFilled();
	unsigned num = std::min(len, available);
	if ((readIdx + num) < mixBufferSize) {
		memcpy(stream, &mixBuffer[readIdx], num * sizeof(int16_t));
		readIdx += num;
	} else {
		unsigned len1 = mixBufferSize - readIdx;
		memcpy(stream, &mixBuffer[readIdx], len1 * sizeof(int16_t));
		unsigned len2 = num - len1;
		memcpy(&stream[len1], &mixBuffer[0], len2 * sizeof(int16_t));
		readIdx = len2;
	}
	int missing = len - available;
	if (missing > 0) {
		// buffer underrun
		memset(&stream[available], 0, missing * sizeof(int16_t));
	}
}

void SDLSoundDriver::uploadBuffer(int16_t* buffer, unsigned len)
{
	SDL_LockAudio();
	len *= 2; // stereo
	unsigned free = getBufferFree();
	if (len > free) {
		if (reactor.getGlobalSettings().getThrottleManager().isThrottled()) {
			do {
				SDL_UnlockAudio();
				Timer::sleep(5000); // 5ms
				SDL_LockAudio();
				if (MSXMotherBoard* board = reactor.getMotherBoard()) {
					board->getRealTime().resync();
				}
				free = getBufferFree();
			} while (len > free);
		} else {
			// drop excess samples
			len = free;
		}
	}
	assert(len <= free);
	if ((writeIdx + len) < mixBufferSize) {
		memcpy(&mixBuffer[writeIdx], buffer, len * sizeof(int16_t));
		writeIdx += len;
	} else {
		unsigned len1 = mixBufferSize - writeIdx;
		memcpy(&mixBuffer[writeIdx], buffer, len1 * sizeof(int16_t));
		unsigned len2 = len - len1;
		memcpy(&mixBuffer[0], &buffer[len1], len2 * sizeof(int16_t));
		writeIdx = len2;
	}

	SDL_UnlockAudio();
}

} // namespace openmsx
