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

	stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &desired, nullptr, nullptr);
	if (!stream) {
		throw MSXException("Unable to open SDL audio: ", SDL_GetError());
	}


	// TODO: Can it happen with SDL3 that our obtained spec differs from the desired spec?
	SDL_AudioSpec obtained;
	SDL_GetAudioStreamFormat(stream, &obtained, nullptr);
	frequency = obtained.freq;
	fragmentSize = frequency / 100; // 10 ms of samples
}

SDLSoundDriver::~SDLSoundDriver()
{
	SDL_DestroyAudioStream(stream);
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

unsigned SDLSoundDriver::getSamplesQueued() const
{
	int bytesQueued = SDL_GetAudioStreamQueued(stream);
	if (bytesQueued == -1) {
		std::cerr << "Failed to get number of queued audio bytes: " << SDL_GetError() << std::endl;
		return 0;
	}
	return bytesQueued / sizeof(StereoFloat);
}

void SDLSoundDriver::uploadBuffer(std::span<const StereoFloat> buffer)
{
	auto targetFill = fragmentSize * 3;
	if (getSamplesQueued() > targetFill) {
		auto* board = reactor.getMotherBoard();
		if (board && !board->getMSXMixer().isSynchronousMode() && // when not recording
		    reactor.getGlobalSettings().getThrottleManager().isThrottled()) {
			do {
				Timer::sleep(5000); // 5ms
				board->getRealTime().resync();
			} while (getSamplesQueued() > targetFill);
		} else {
			// drop excess samples
			return;
		}
	}

	if (!SDL_PutAudioStreamData(stream, buffer.data(), buffer.size_bytes())) {
		std::cerr << "Failed to enqueue audio: " << SDL_GetError() << std::endl;
	}
}

} // namespace openmsx
