#include "PASoundDriver.hh"
#if COMPONENT_PORTAUDIO

#include "Reactor.hh"
#include "MSXMotherBoard.hh"
#include "RealTime.hh"
#include "GlobalSettings.hh"
#include "ThrottleManager.hh"
#include "MSXException.hh"

namespace openmsx {

PASoundDriver::PASoundDriver(Reactor& reactor_,
                             unsigned wantedFreq, unsigned wantedSamples)
	: reactor(reactor_)
	, samples(wantedSamples)
	, muted(true)
{
	PaError err = Pa_Initialize();
	if (err != paNoError) {
		throw MSXException("Unable to open PA audio: ", Pa_GetErrorText(err));
	}

	PaStreamParameters outputParameters;
	outputParameters.device = Pa_GetDefaultOutputDevice();
	if (outputParameters.device == paNoDevice) {
		Pa_Terminate();
		throw MSXException("No default audio output device.");
	}

	outputParameters.channelCount = 2;		// stereo
	outputParameters.sampleFormat = paFloat32;	// 32 bit floating point
	outputParameters.suggestedLatency =
		Pa_GetDeviceInfo(outputParameters.device)->defaultLowOutputLatency;
	outputParameters.hostApiSpecificStreamInfo = nullptr;

	err = Pa_OpenStream(&stream,
	                    nullptr,		// no input channels
	                    &outputParameters,
	                    wantedFreq,
	                    wantedSamples,
	                    paClipOff | paDitherOff,
	                    nullptr,
	                    nullptr);
	if (err != paNoError) {
		Pa_Terminate();
		throw MSXException("Unable to open audio stream: ", Pa_GetErrorText(err));
	}

	frequency = Pa_GetStreamInfo(stream)->sampleRate;
}

PASoundDriver::~PASoundDriver()
{
	Pa_CloseStream(stream);
	Pa_Terminate();
}

void PASoundDriver::mute()
{
	if (!muted) {
		muted = true;
		Pa_AbortStream(stream);
	}
}

void PASoundDriver::unmute()
{
	if (muted) {
		muted = false;
		Pa_StartStream(stream);
	}
}

unsigned PASoundDriver::getFrequency() const
{
	return frequency;
}

unsigned PASoundDriver::getSamples() const
{
	return samples;
}

void PASoundDriver::uploadBuffer(float* buffer, unsigned len)
{
	auto free = Pa_GetStreamWriteAvailable(stream);
	if (len > free) {
		if (reactor.getGlobalSettings().getThrottleManager().isThrottled()) {
			// play excess samples and resynchronise
			Pa_WriteStream(stream, buffer, len);
			if (MSXMotherBoard* board = reactor.getMotherBoard()) {
				board->getRealTime().resync();
			}
		} else {
			// drop excess samples
			Pa_WriteStream(stream, buffer, free);
		}
	} else {
		auto err = Pa_WriteStream(stream, buffer, len);
		if (err == paOutputUnderflowed) {
			// avoid perpetual underflow by filling the stream more
			int i = Pa_GetStreamWriteAvailable(stream) / len;
			while (--i > 0) Pa_WriteStream(stream, buffer, len);
		}
	}
}

} // namespace openmsx

#endif
