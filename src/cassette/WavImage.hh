// $Id$

#ifndef __WAVIMAGE_HH__
#define __WAVIMAGE_HH__

#include <SDL/SDL.h>
#include <string>
#include "CassetteImage.hh"
#include "openmsx.hh"

using std::string;

class FileContext;


class WavImage : public CassetteImage
{
	public:
		WavImage(FileContext *context, const string &fileName);
		virtual ~WavImage();

		virtual short getSampleAt(float pos);

	private:
		SDL_AudioSpec audioSpec;
		Uint32 audioLength;	// 0 means no tape inserted
		Uint8 *audioBuffer;	// 0 means no tape inserted
};

#endif
