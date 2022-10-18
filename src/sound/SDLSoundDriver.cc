#include "SDLSoundDriver.hh"
#include "Mixer.hh"
#include "Reactor.hh"
#include "MSXMixer.hh"
#include "MSXMotherBoard.hh"
#include "RealTime.hh"
#include "GlobalSettings.hh"
#include "ThrottleManager.hh"
#include "MSXException.hh"
#include "Timer.hh"
#include <algorithm>
#include <bit>
#include <cassert>

namespace openmsx {

SDLSoundDriver::SDLSoundDriver(Reactor& reactor_,
                               unsigned wantedFreq, unsigned wantedSamples)
	: reactor(reactor_)
	, muted(true)
{
	SDL_AudioSpec desired;
	desired.freq     = wantedFreq;
	desired.samples  = std::bit_ceil(wantedSamples);
	desired.channels = 2; // stereo
	desired.format   = AUDIO_F32SYS;
	desired.callback = audioCallbackHelper; // must be a static method
	desired.userdata = this;

	SDL_AudioSpec obtained;
	deviceID = SDL_OpenAudioDevice(nullptr, SDL_FALSE, &desired, &obtained,
	                               SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
	if (!deviceID) {
		throw MSXException("Unable to open SDL audio: ", SDL_GetError());
	}

	frequency = obtained.freq;
	fragmentSize = obtained.samples;

	mixBufferSize = 3 * (obtained.size / sizeof(float)) + 2;
	mixBuffer.resize(mixBufferSize);
	reInit();
}

SDLSoundDriver::~SDLSoundDriver()
{
	SDL_CloseAudioDevice(deviceID);
}

void SDLSoundDriver::reInit()
{
	SDL_LockAudioDevice(deviceID);
	readIdx  = 0;
	writeIdx = 0;
	SDL_UnlockAudioDevice(deviceID);
}

void SDLSoundDriver::mute()
{
	if (!muted) {
		muted = true;
		SDL_PauseAudioDevice(deviceID, 1);
	}
}

void SDLSoundDriver::unmute()
{
	if (muted) {
		muted = false;
		reInit();
		SDL_PauseAudioDevice(deviceID, 0);
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

void SDLSoundDriver::audioCallbackHelper(void* userdata, uint8_t* strm, int len)
{
	assert((len & 7) == 0); // stereo, 32 bit float
	static_cast<SDLSoundDriver*>(userdata)->
		audioCallback(reinterpret_cast<float*>(strm), len / sizeof(float));
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

void SDLSoundDriver::audioCallback(float* stream_, unsigned len)
{
	assert((len & 1) == 0); // stereo
	len /= 2;
	auto* stream = reinterpret_cast<StereoFloat*>(stream_);

	unsigned available = getBufferFilled();
	unsigned num = std::min(len, available);
	if ((readIdx + num) < mixBufferSize) {
		ranges::copy(std::span{&mixBuffer[readIdx], num}, stream);
		readIdx += num;
	} else {
		unsigned len1 = mixBufferSize - readIdx;
		ranges::copy(std::span{&mixBuffer[readIdx], len1}, stream);
		unsigned len2 = num - len1;
		ranges::copy(std::span{&mixBuffer[0], len2}, &stream[len1]);
		readIdx = len2;
	}
	int missing = len - available;
	if (missing > 0) {
		// buffer underrun
		ranges::fill(std::span{&stream[available], size_t(missing)}, StereoFloat{});
	}
}

void SDLSoundDriver::uploadBuffer(std::span<const StereoFloat> buffer)
{
	SDL_LockAudioDevice(deviceID);
	unsigned len = buffer.size();
	unsigned free = getBufferFree();
	if (len > free) {
		auto* board = reactor.getMotherBoard();
		if (board && !board->getMSXMixer().isSynchronousMode() && // when not recording
		    reactor.getGlobalSettings().getThrottleManager().isThrottled()) {
			do {
				SDL_UnlockAudioDevice(deviceID);
				Timer::sleep(5000); // 5ms
				SDL_LockAudioDevice(deviceID);
				board->getRealTime().resync();
				free = getBufferFree();
			} while (len > free);
		} else {
			// drop excess samples
			len = free;
		}
	}
	assert(len <= free);
	if ((writeIdx + len) < mixBufferSize) {
		ranges::copy(buffer, &mixBuffer[writeIdx]);
		writeIdx += len;
	} else {
		unsigned len1 = mixBufferSize - writeIdx;
		ranges::copy(buffer.subspan(0, len1), &mixBuffer[writeIdx]);
		unsigned len2 = len - len1;
		ranges::copy(buffer.subspan(len1, len2), &mixBuffer[0]);
		writeIdx = len2;
	}

	SDL_UnlockAudioDevice(deviceID);
}

} // namespace openmsx
