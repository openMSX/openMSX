// $Id$

#ifndef __WAVIMAGE_HH__
#define __WAVIMAGE_HH__

#include "CassetteImage.hh"
#include "EmuTime.hh"
#include <string>

namespace openmsx {

class WavImage : public CassetteImage
{
public:
	WavImage(const std::string& fileName);
	virtual ~WavImage();

	virtual short getSampleAt(const EmuTime& time);

private:
	int length;
	byte* buffer;
	DynamicClock clock;
	short average;
};

} // namespace openmsx

#endif
