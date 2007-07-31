// $Id$

#ifndef WAVIMAGE_HH
#define WAVIMAGE_HH

#include "CassetteImage.hh"
#include "DynamicClock.hh"
#include "noncopyable.hh"
#include <string>

namespace openmsx {

class WavImage : public CassetteImage, private noncopyable
{
public:
	explicit WavImage(const std::string& fileName);
	virtual ~WavImage();

	virtual short getSampleAt(const EmuTime& time);
	virtual EmuTime getEndTime() const;
	virtual unsigned getFrequency() const;
	virtual void fillBuffer(unsigned pos, int** bufs, unsigned num) const;

private:
	int getSample(unsigned pos) const;

	unsigned nbSamples;
	byte* buffer;
	DynamicClock clock;
	short average;
};

} // namespace openmsx

#endif
