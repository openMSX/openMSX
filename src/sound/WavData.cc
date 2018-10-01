#include "WavData.hh"
#include "MSXException.hh"
#include "SDLSurfacePtr.hh"
#include <cassert>

using std::string;

namespace openmsx {

static inline bool is8Bit(Uint16 format)
{
	return (format == AUDIO_U8) || (format == AUDIO_S8);
}

WavData::WavData(const string& filename, unsigned wantedBits, unsigned wantedFreq)
{
	SDL_AudioSpec wavSpec;
	Uint8* wavBuf_;
	Uint32 wavLen;
	if (!SDL_LoadWAV(filename.c_str(), &wavSpec, &wavBuf_, &wavLen)) {
		throw MSXException("WavData error: ", SDL_GetError());
	}
	SDLWavPtr wavBuf(wavBuf_);

	freq = (wantedFreq == 0) ? unsigned(wavSpec.freq) : wantedFreq;
	bits = (wantedBits == 0) ? (is8Bit(wavSpec.format) ? 8 : 16)
	                         : wantedBits;
	assert((bits == 8) || (bits == 16));
	Uint16 format = (bits == 8) ? AUDIO_U8 : AUDIO_S16SYS;

	SDL_AudioCVT audioCVT;
	if (SDL_BuildAudioCVT(&audioCVT, wavSpec.format, wavSpec.channels,
		              wavSpec.freq, format, 1, freq) == -1) {
		throw MSXException("Couldn't build wav converter");
	}

	buffer.resize(wavLen * audioCVT.len_mult);
	memcpy(buffer.data(), wavBuf.get(), wavLen);
	audioCVT.buf = buffer.data();
	audioCVT.len = wavLen;

	if (SDL_ConvertAudio(&audioCVT) == -1) {
		throw MSXException("Couldn't convert wav file to internal format");
	}
	length = unsigned(audioCVT.len * audioCVT.len_ratio) / 2;
}

} // namespace openmsx
