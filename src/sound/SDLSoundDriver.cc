#include "SDLSoundDriver.hh"

#include "MSXMixer.hh"
#include "Mixer.hh"

#include "GlobalSettings.hh"
#include "MSXException.hh"
#include "MSXMotherBoard.hh"
#include "Reactor.hh"
#include "RealTime.hh"
#include "ThrottleManager.hh"
#include "Timer.hh"

#include "narrow.hh"

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <string>

namespace openmsx {

SDLSoundDriver::SDLSoundDriver(Reactor& reactor_,
                               unsigned wantedFreq, unsigned wantedSamples)
	: reactor(reactor_)
{
	auto wantedFrames = std::bit_ceil(wantedSamples);
	// The size of the device buffer (in sample frames). SDL may not
	// (exactly) honor this on all platforms.
	SDL_SetHint(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES, std::to_string(wantedFrames).c_str());

	SDL_AudioSpec spec;
	spec.format   = SDL_AUDIO_F32;
	spec.channels = 2; // stereo
	spec.freq     = narrow<int>(wantedFreq);
	// Like with SDL2, the device starts paused and our callback provides
	// the data whenever the device needs it. SDL converts the data to the
	// format and frequency of the device, if needed.
	stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec,
	                                   audioCallbackHelper, this); // must be a static method
	if (!stream) {
		throw MSXException("Unable to open SDL audio: ", SDL_GetError());
	}

	frequency = wantedFreq;
	// Like with SDL2, base our own buffer size on the 'samples' setting,
	// not on the actual device buffer: e.g. WASAPI on Windows uses a much
	// smaller device buffer (the period of the audio engine, typically
	// 10ms), but our buffer must still absorb the hiccups of the main
	// thread. A larger device buffer would add latency on top of ours.
	fragmentSize = wantedFrames;

	mixBuffer.resize(3 * fragmentSize + 1);
	reInit();
}

SDLSoundDriver::~SDLSoundDriver()
{
	SDL_DestroyAudioStream(stream); // also closes the device
}

void SDLSoundDriver::reInit()
{
	SDL_LockAudioStream(stream);
	readIdx  = 0;
	writeIdx = 0;
	SDL_ClearAudioStream(stream); // also drop what SDL still has buffered
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

void SDLSoundDriver::audioCallbackHelper(void* userdata, SDL_AudioStream* strm,
                                         int additionalAmount, int /*totalAmount*/)
{
	// 'additionalAmount' is in bytes and may be slightly overestimated
	if (additionalAmount <= 0) return;
	auto len = (size_t(additionalAmount) + sizeof(StereoFloat) - 1) / sizeof(StereoFloat);
	static_cast<SDLSoundDriver*>(userdata)->audioCallback(strm, len);
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

void SDLSoundDriver::audioCallback(SDL_AudioStream* strm, size_t len)
{
	// Note: SDL already holds the lock of the stream during this callback.
	auto put = [&](std::span<const StereoFloat> data) {
		if (data.empty()) return;
		SDL_PutAudioStreamData(strm, data.data(), narrow<int>(data.size_bytes()));
	};

	size_t available = getBufferFilled();
	if (auto num = std::min(len, available);
	    (readIdx + num) < mixBuffer.size()) {
		put(mixBuffer.subspan(readIdx, num));
		readIdx += narrow<unsigned>(num);
	} else {
		auto len1 = mixBuffer.size() - readIdx;
		put(mixBuffer.subspan(readIdx, len1));
		auto len2 = num - len1;
		put(mixBuffer.first(len2));
		readIdx = narrow<unsigned>(len2);
	}
	if (len > available) {
		// buffer underrun
		static constexpr std::array<StereoFloat, 256> silence = {};
		for (auto missing = len - available; missing > 0;) {
			auto n = std::min(missing, silence.size());
			put(std::span{silence}.first(n));
			missing -= n;
		}
	}
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
