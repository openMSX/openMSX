// $Id$

#include "WavAudioInput.hh"
#include "EmuTime.hh"
#include "MSXException.hh"
#include "CliCommOutput.hh"

namespace openmsx {

WavAudioInput::WavAudioInput()
	: length(0), buffer(0), freq(44100)
	, plugged(false)
	, audioInputFilenameSetting("audio-inputfilename",
		"filename of the file where the sampler reads data from",
		"audio-input.wav")
{
	audioInputFilenameSetting.addListener(this);
}


WavAudioInput::~WavAudioInput()
{
	audioInputFilenameSetting.removeListener(this);
	freeWave();
}

void WavAudioInput::loadWave()
	throw(MSXException)
{
	freeWave();

	SDL_AudioSpec wavSpec;
	Uint8 *wavBuf;
	Uint32 wavLen;
	if (SDL_LoadWAV(audioInputFilenameSetting.getValueString().c_str(), &wavSpec, &wavBuf, &wavLen) == NULL) {
		throw MSXException(string("WavAudioInput error: ") + SDL_GetError());
	}

	freq = wavSpec.freq;
	SDL_AudioCVT audioCVT;
	if (SDL_BuildAudioCVT(&audioCVT,
			wavSpec.format, wavSpec.channels, freq,
			AUDIO_S16,      1,                freq) == -1) {
		SDL_FreeWAV(wavBuf);
		throw MSXException("Couldn't build wav converter");
	}

	buffer = (Uint8 *)malloc(wavLen * audioCVT.len_mult);
	audioCVT.buf = buffer;
	audioCVT.len = wavLen;
	memcpy(buffer, wavBuf, wavLen);
	SDL_FreeWAV(wavBuf);

	if (SDL_ConvertAudio(&audioCVT) == -1) {
		freeWave();
		throw MSXException("Couldn't convert wav file to internal format");
	}
	length = (int)(audioCVT.len * audioCVT.len_ratio) / 2;
}

void WavAudioInput::freeWave()
{
	if (buffer != 0) {
		free(buffer);
	}
	length = 0;
	buffer = 0;
	freq = 44100;
}

const string& WavAudioInput::getName() const
{
	static const string name("wavinput");
	return name;
}

const string& WavAudioInput::getDescription() const
{
	static const string desc(
		"Read .wav files. Can for example be used as input for "
		"samplers.");
	return desc;
}

void WavAudioInput::plug(Connector *connector, const EmuTime &time)
	throw(PlugException)
{
	if (buffer == 0) {
		try {
			loadWave();
		} catch (MSXException &e) {
			throw PlugException("Load of wave file failed: " + e.getMessage());
		}
	}
	reference = time;
	plugged = true;
}

void WavAudioInput::unplug(const EmuTime &time)
{
	freeWave();
	plugged = false;
}

void WavAudioInput::update(const SettingLeafNode *setting)
{
	assert (setting == &audioInputFilenameSetting);
	if (plugged) {
		try {
			loadWave();
		} catch (MSXException &e) {
			// TODO proper error handling, message should go to console
			CliCommOutput::instance().printWarning(
				"Load of wave file failed: " + e.getMessage());
		}
	}
}

short WavAudioInput::readSample(const EmuTime &time)
{
	int pos = (time - reference).getTicksAt(freq);
	if (pos < length) {
		return ((short *)buffer)[pos];
	} else {
		return 0;
	}
}

} // namespace openmsx
