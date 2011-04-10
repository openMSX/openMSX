// $Id$

#include "SDLSoundDriver.hh"
#include "MSXException.hh"
#include "Math.hh"
#include "StringOp.hh"
#include "build-info.hh"
#include <SDL.h>
#include <algorithm>
#include <cassert>
#include <cstring>

namespace openmsx {

SDLSoundDriver::SDLSoundDriver(unsigned wantedFreq, unsigned wantedSamples)
	: muted(true)
{
	SDL_AudioSpec desired;
	desired.freq     = wantedFreq;
	desired.samples  = Math::powerOfTwo(wantedSamples);
	desired.channels = 2; // stereo
	desired.format   = OPENMSX_BIGENDIAN ? AUDIO_S16MSB : AUDIO_S16LSB;
	desired.callback = audioCallbackHelper; // must be a static method
	desired.userdata = this;

	if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
		throw MSXException(StringOp::Builder()
			<< "Unable to initialize SDL audio subsystem: "
			<< SDL_GetError());
	}
	SDL_AudioSpec audioSpec;
	if (SDL_OpenAudio(&desired, &audioSpec) != 0) {
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
		throw MSXException(StringOp::Builder()
			<< "Unable to open SDL audio: " << SDL_GetError());
	}
	//std::cerr << "DEBUG wanted: " << wantedSamples
	//          <<     "  actual: " << audioSpec.size / 4 << std::endl;
	frequency = audioSpec.freq;
	bufferSize = 4 * (audioSpec.size / sizeof(short));

	fragmentSize = 256;
	while ((bufferSize / fragmentSize) >= 32) {
		fragmentSize *= 2;
	}
	while ((bufferSize / fragmentSize) < 8) {
		fragmentSize /= 2;
	}

	mixBuffer.resize(bufferSize);
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
	memset(mixBuffer.data(), 0, bufferSize * sizeof(short));
	readIdx  = 0;
	writeIdx = (5 * bufferSize) / 8;
	filledStat = 1.0;
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
		audioCallback(reinterpret_cast<short*>(strm), len / sizeof(short));
}

unsigned SDLSoundDriver::getBufferFilled() const
{
	int result = writeIdx - readIdx;
	if (result < 0) result += bufferSize;
	assert((0 <= result) && (unsigned(result) < bufferSize));
	return result;
}

unsigned SDLSoundDriver::getBufferFree() const
{
	// we can't distinguish completely filled from completely empty
	// (in both cases readIx would be equal to writeIdx), so instead
	// we define full as '(writeIdx + 2) == readIdx' (note that index
	// increases in steps of 2 (stereo)).
	int result = bufferSize - 2 - getBufferFilled();
	assert((0 <= result) && (unsigned(result) < bufferSize));
	return result;
}

void SDLSoundDriver::audioCallback(short* stream, unsigned len)
{
	assert((len & 1) == 0); // stereo
	unsigned available = getBufferFilled();
	//PRT_DEBUG("DEBUG callback: " << available);
	unsigned num = std::min(len, available);
	if ((readIdx + num) < bufferSize) {
		memcpy(stream, &mixBuffer[readIdx], num * sizeof(short));
		readIdx += num;
	} else {
		unsigned len1 = bufferSize - readIdx;
		memcpy(stream, &mixBuffer[readIdx], len1 * sizeof(short));
		unsigned len2 = num - len1;
		memcpy(&stream[len1], &mixBuffer[0], len2 * sizeof(short));
		readIdx = len2;
	}
	int missing = len - available;
	if (missing > 0) {
		// buffer underrun
		//PRT_DEBUG("DEBUG underrun: " << missing);
		memset(&stream[available], 0, missing * sizeof(short));
	}

	unsigned target = (5 * bufferSize) / 8;
	double factor = double(available) / target;
	filledStat = (63 * filledStat + factor) / 64;
	//PRT_DEBUG("DEBUG filledStat: " << filledStat);
}

double SDLSoundDriver::uploadBuffer(short* buffer, unsigned len)
{
	SDL_LockAudio();
	len *= 2; // stereo
	unsigned free = getBufferFree();
	//if (len > free) {
	//	PRT_DEBUG("DEBUG overrun: " << len - free);
	//}
	unsigned num = std::min(len, free); // ignore overrun (drop samples)
	if ((writeIdx + num) < bufferSize) {
		memcpy(&mixBuffer[writeIdx], buffer, num * sizeof(short));
		writeIdx += num;
	} else {
		unsigned len1 = bufferSize - writeIdx;
		memcpy(&mixBuffer[writeIdx], buffer, len1 * sizeof(short));
		unsigned len2 = num - len1;
		memcpy(&mixBuffer[0], &buffer[len1], len2 * sizeof(short));
		writeIdx = len2;
	}

	//unsigned available = getBufferFilled();
	double result = filledStat;
	filledStat = 1.0; // only report difference once
	SDL_UnlockAudio();
	return result;
}

} // namespace openmsx
