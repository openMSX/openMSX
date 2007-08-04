// $Id: $

#include "WavData.hh"
#include "MSXException.hh"
#include <SDL.h>
#include <cstdlib>
#include <cassert>

using std::string;

namespace openmsx {

bool is8Bit(Uint16 format)
{
	return (format == AUDIO_U8) || (format == AUDIO_S8);
}

WavData::WavData(const string& filename, int wantedBits, int wantedFreq)
{
	SDL_AudioSpec wavSpec;
	Uint8* wavBuf;
	Uint32 wavLen;
	if (SDL_LoadWAV(filename.c_str(), &wavSpec, &wavBuf, &wavLen) == NULL) {
		throw MSXException(string("WavData error: ") +
		                   SDL_GetError());
	}

	freq = (wantedFreq == 0) ? wavSpec.freq : wantedFreq;
	bits = (wantedBits == 0) ? (is8Bit(wavSpec.format) ? 8 : 16)
	                         : wantedBits;
	assert((bits == 8) || (bits == 16));
	Uint16 format = (bits == 8) ? AUDIO_U8 : AUDIO_S16SYS;

	SDL_AudioCVT audioCVT;
	if (SDL_BuildAudioCVT(&audioCVT, wavSpec.format, wavSpec.channels,
		              wavSpec.freq, format, 1, freq) == -1) {
		SDL_FreeWAV(wavBuf);
		throw MSXException("Couldn't build wav converter");
	}

	buffer = malloc(wavLen * audioCVT.len_mult);
	audioCVT.buf = static_cast<Uint8*>(buffer);
	audioCVT.len = wavLen;
	memcpy(buffer, wavBuf, wavLen);
	SDL_FreeWAV(wavBuf);

	if (SDL_ConvertAudio(&audioCVT) == -1) {
		free(buffer);
		throw MSXException("Couldn't convert wav file to internal format");
	}
	length = int(audioCVT.len * audioCVT.len_ratio) / 2;
}

WavData::~WavData()
{
	free(buffer);
}

unsigned WavData::getFreq() const
{
	return freq;
}

unsigned WavData::getBits() const
{
	return bits;
}

unsigned WavData::getSize() const
{
	return length;
}

const void* WavData::getData() const
{
	return buffer;
}

} // namespace openmsx
