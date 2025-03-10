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
                               unsigned wantedFreq, unsigned /*wantedSamples*/)
	: reactor(reactor_)
{
	SDL_AudioSpec desired;
	desired.freq     = narrow<int>(wantedFreq);
	desired.channels = 2; // stereo
	desired.format   = SDL_AUDIO_F32;

	stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &desired, audioCallbackHelper, this);
	if (!stream) {
		throw MSXException("Unable to open SDL audio: ", SDL_GetError());
	}


	// TODO: Can it happen with SDL3 that our obtained spec differs from the desired spec?
	SDL_AudioSpec obtained;
	SDL_GetAudioStreamFormat(stream, &obtained, nullptr);
	frequency = obtained.freq;
	fragmentSize = 1024; // TODO: We should probably rethink the whole audio driver.

	mixBuffer.resize(3 * fragmentSize);
	reInit();
}

SDLSoundDriver::~SDLSoundDriver()
{
	SDL_DestroyAudioStream(stream);
}

void SDLSoundDriver::reInit()
{
	SDL_LockAudioStream(stream);
	readIdx  = 0;
	writeIdx = 0;
	SDL_UnlockAudioStream(stream);
}

void SDLSoundDriver::mute()
{
	if (!muted) {
		muted = true;
		SDL_PauseAudioStreamDevice(stream);
	}
}

void SDLSoundDriver::unmute()
{
	if (muted) {
		muted = false;
		reInit();
		SDL_ResumeAudioStreamDevice(stream);
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

void SDLSoundDriver::audioCallbackHelper(void* userdata, SDL_AudioStream* stream, int additional_amount, int total_amount)
{
	#if 0
	assert((len & 7) == 0); // stereo, 32 bit float
	static_cast<SDLSoundDriver*>(userdata)->
		audioCallback(stream, len / (2 * sizeof(float))});
	#endif
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

void SDLSoundDriver::audioCallback(/*std::span<StereoFloat> stream*/)
{
	#if 0
	auto len = stream.size();

	size_t available = getBufferFilled();
	if (auto num = std::min(len, available);
	    (readIdx + num) < mixBuffer.size()) {
		copy_to_range(mixBuffer.subspan(readIdx, num), stream);
		readIdx += narrow<unsigned>(num);
	} else {
		auto len1 = mixBuffer.size() - readIdx;
		copy_to_range(mixBuffer.subspan(readIdx, len1), stream);
		auto len2 = num - len1;
		copy_to_range(mixBuffer.first(len2), stream.subspan(len1));
		readIdx = narrow<unsigned>(len2);
	}
	auto missing = narrow_cast<ptrdiff_t>(len - available);
	if (missing > 0) {
		// buffer underrun
		std::ranges::fill(subspan(stream, available, missing), StereoFloat{});
	}
	#endif
}

void SDLSoundDriver::uploadBuffer(std::span<const StereoFloat> buffer)
{
	SDL_LockAudioStream(stream);
	unsigned free = getBufferFree();
	if (buffer.size() > free) {
		auto* board = reactor.getMotherBoard();
		if (board && !board->getMSXMixer().isSynchronousMode() && // when not recording
		    reactor.getGlobalSettings().getThrottleManager().isThrottled()) {
			do {
				SDL_UnlockAudioStream(stream);
				Timer::sleep(5000); // 5ms
				SDL_LockAudioStream(stream);
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
		copy_to_range(buffer, mixBuffer.subspan(writeIdx));
		writeIdx += narrow<unsigned>(buffer.size());
	} else {
		auto len1 = mixBuffer.size() - writeIdx;
		copy_to_range(buffer.subspan(0, len1), mixBuffer.subspan(writeIdx));
		auto len2 = buffer.size() - len1;
		copy_to_range(buffer.subspan(len1, len2), std::span{mixBuffer});
		writeIdx = narrow<unsigned>(len2);
	}

	SDL_UnlockAudioStream(stream);
}

} // namespace openmsx
