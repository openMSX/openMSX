// $Id$

#include "WavAudioInput.hh"
#include "EmuTime.hh"
#include "PluggingController.hh"


WavAudioInput::WavAudioInput()
	: length(0), buffer(0), freq(44100)
{
	SDL_AudioSpec wavSpec;
	Uint8* wavBuf;
	Uint32 wavLen;
	if (SDL_LoadWAV("audio-input.wav", &wavSpec, &wavBuf, &wavLen) == NULL) {
		PRT_DEBUG("WavAudioInput error: " << SDL_GetError());
	}
	
	freq = wavSpec.freq;
	SDL_AudioCVT audioCVT;
	if (SDL_BuildAudioCVT(&audioCVT,
		              wavSpec.format, wavSpec.channels, freq,
			      AUDIO_S16,      1,                freq) == -1) {
		SDL_FreeWAV(wavBuf);
		PRT_DEBUG("Couldn't build wav converter");
	}
	
	buffer = (Uint8*)malloc(wavLen * audioCVT.len_mult);
	audioCVT.buf = buffer;
	audioCVT.len = wavLen;
	memcpy(buffer, wavBuf, wavLen);
	SDL_FreeWAV(wavBuf);

	if (SDL_ConvertAudio(&audioCVT) == -1) {
		PRT_DEBUG("Couldn't convert wav");
	}
	length = (int)(audioCVT.len * audioCVT.len_ratio) / 2;

	PluggingController::instance()->registerPluggable(this);
}

WavAudioInput::~WavAudioInput()
{
	PluggingController::instance()->unregisterPluggable(this);
	if (buffer) {
		free(buffer);
	}
}

const string& WavAudioInput::getName() const
{
	static const string name("wavinput");
	return name;
}

void WavAudioInput::plug(Connector* connector, const EmuTime &time)
{
	reference = time;
}

void WavAudioInput::unplug(const EmuTime &time)
{
}

short WavAudioInput::readSample(const EmuTime &time)
{
	int pos = (time - reference).getTicksAt(freq);
	if (pos < length) {
		return ((short*)buffer)[pos];
	} else {
		return 0;
	}
}
