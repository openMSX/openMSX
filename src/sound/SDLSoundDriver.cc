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

#include "narrow.hh"

#include <algorithm>
#include <bit>
#include <cassert>

namespace openmsx {

SDLSoundDriver::SDLSoundDriver(Reactor& reactor_,
                               unsigned wantedFreq, unsigned wantedSamples)
	: reactor(reactor_)
{
	SDL_AudioSpec desired;
	desired.freq     = narrow<int>(wantedFreq);
	desired.samples  = narrow<Uint16>(std::bit_ceil(wantedSamples));
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

	mixBuffer.resize(3 * (obtained.size / sizeof(StereoFloat)) + 1);
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
		audioCallback(std::span{std::bit_cast<StereoFloat*>(strm),
		                        len / (2 * sizeof(float))});
}

unsigned SDLSoundDriver::getBufferFilled() const
{
	int result = narrow_cast<int>(writeIdx - readIdx);
	if (result < 0) result += narrow<int>(mixBuffer.size());
	assert((0 <= result) && (narrow<unsigned>(result) < mixBuffer.size()));
	return result;
}

unsigned SDLSoundDriver::getBufferFree() const
{
	// we can't distinguish completely filled from completely empty
	// (in both cases readIx would be equal to writeIdx), so instead
	// we define full as '(writeIdx + 1) == readIdx'.
	auto result = narrow<unsigned>(mixBuffer.size() - 1 - getBufferFilled());
	assert(narrow_cast<int>(result) >= 0);
	assert(result < mixBuffer.size());
	return result;
}

void SDLSoundDriver::audioCallback(std::span<StereoFloat> stream)
{
	auto len = stream.size();

	size_t available = getBufferFilled();
	auto num = std::min(len, available);
	if ((readIdx + num) < mixBuffer.size()) {
		ranges::copy(mixBuffer.subspan(readIdx, num), stream);
		readIdx += num;
	} else {
		auto len1 = mixBuffer.size() - readIdx;
		ranges::copy(mixBuffer.subspan(readIdx, len1), stream);
		auto len2 = num - len1;
		ranges::copy(mixBuffer.first(len2), stream.subspan(len1));
		readIdx = narrow<unsigned>(len2);
	}
	auto missing = narrow_cast<ptrdiff_t>(len - available);
	if (missing > 0) {
		// buffer underrun
		ranges::fill(subspan(stream, available, missing), StereoFloat{});
	}
}

void SDLSoundDriver::uploadBuffer(std::span<const StereoFloat> buffer)
{
	SDL_LockAudioDevice(deviceID);
	unsigned free = getBufferFree();
	if (buffer.size() > free) {
		auto* board = reactor.getMotherBoard();
		if (board && !board->getMSXMixer().isSynchronousMode() && // when not recording
		    reactor.getGlobalSettings().getThrottleManager().isThrottled()) {
			do {
				SDL_UnlockAudioDevice(deviceID);
				Timer::sleep(5000); // 5ms
				SDL_LockAudioDevice(deviceID);
				board->getRealTime().resync();
				free = getBufferFree();
			} while (buffer.size() > free);
		} else {
			// drop excess samples
			buffer = buffer.subspan(0, free);
		}
	}
	assert(buffer.size() <= free);
	if ((writeIdx + buffer.size()) < mixBuffer.size()) {
		ranges::copy(buffer, mixBuffer.subspan(writeIdx));
		writeIdx += narrow<unsigned>(buffer.size());
	} else {
		auto len1 = mixBuffer.size() - writeIdx;
		ranges::copy(buffer.subspan(0, len1), mixBuffer.subspan(writeIdx));
		auto len2 = buffer.size() - len1;
		ranges::copy(buffer.subspan(len1, len2), std::span{mixBuffer});
		writeIdx = narrow<unsigned>(len2);
	}

	SDL_UnlockAudioDevice(deviceID);
}

} // namespace openmsx
