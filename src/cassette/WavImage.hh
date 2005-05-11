// $Id$

#ifndef WAVIMAGE_HH
#define WAVIMAGE_HH

#include "CassetteImage.hh"
#include "DynamicClock.hh"
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
