// $Id$

#include "WavImage.hh"
#include "MSXException.hh"
#include "File.hh"
#include "Math.hh"
#include <SDL.h>

using std::string;

namespace openmsx {

// Note: type detection not implemented yet for WAV images
WavImage::WavImage(const string& fileName)
	: nbSamples(0), buffer(0), clock(EmuTime::zero)
{
	File file(fileName);
	const char* name = file.getLocalName().c_str();

	SDL_AudioSpec wavSpec;
	Uint8* wavBuf;
	Uint32 wavLen;
	if (SDL_LoadWAV(name, &wavSpec, &wavBuf, &wavLen) == NULL) {
		string msg = string("CassettePlayer error: ") + SDL_GetError();
		throw MSXException(msg);
	}

	clock.setFreq(wavSpec.freq);
	SDL_AudioCVT audioCVT;
	if (SDL_BuildAudioCVT(
			&audioCVT, wavSpec.format, wavSpec.channels, wavSpec.freq,
			AUDIO_S16, 1, wavSpec.freq) == -1) {
		SDL_FreeWAV(wavBuf);
		throw MSXException("Couldn't build wav converter");
	}

	buffer = static_cast<byte*>(malloc(wavLen * audioCVT.len_mult));
	audioCVT.buf = buffer;
	audioCVT.len = wavLen;
	memcpy(buffer, wavBuf, wavLen);
	SDL_FreeWAV(wavBuf);

	if (SDL_ConvertAudio(&audioCVT) == -1) {
		free(buffer);
		throw MSXException("Couldn't convert wav");
	}
	nbSamples = static_cast<unsigned>(audioCVT.len * audioCVT.len_ratio) / 2;

	// calculate the average to subtract it later (simple DC filter)
	if (nbSamples > 0) {
		long long total = 0;
		for (unsigned i = 0; i < nbSamples; ++i) {
			total += getSample(i);
		}
		average = static_cast<short>(total / nbSamples);
	} else {
		average = 0;
	}
}

WavImage::~WavImage()
{
	free(buffer);
}

int WavImage::getSample(unsigned pos) const
{
	return (pos < nbSamples)
		? buffer[pos * 2] +
		  (static_cast<signed char>(buffer[pos * 2 + 1]) << 8)
		: 0;
}

short WavImage::getSampleAt(const EmuTime& time)
{
	unsigned pos = clock.getTicksTill(time);
	return Math::clipIntToShort(getSample(pos) - average);
}

EmuTime WavImage::getEndTime() const
{
	DynamicClock clk(clock);
	clk += nbSamples;
	return clk.getTime();
}

unsigned WavImage::getFrequency() const
{
	return clock.getFreq();
}

void WavImage::fillBuffer(unsigned pos, int** bufs, unsigned num) const
{
	if (pos < nbSamples) {
		for (unsigned i = 0; i < num; ++i) {
			bufs[0][i] = getSample(pos + i);
		}
	} else {
		bufs[0] = 0;
	}
}

} // namespace openmsx
