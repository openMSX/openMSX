// $Id$

#ifndef __WAVIMAGE_HH__
#define __WAVIMAGE_HH__

#include <SDL/SDL.h>
#include <string>
#include "CassetteImage.hh"
#include "openmsx.hh"

using std::string;

namespace openmsx {

class FileContext;


class WavImage : public CassetteImage
{
	public:
		WavImage(FileContext &context, const string &fileName);
		virtual ~WavImage();

		virtual short getSampleAt(const EmuTime &time);

	private:
		int length;
		Uint8* buffer;
		int freq;
};

} // namespace openmsx

#endif
