#ifndef WAVIMAGE_HH
#define WAVIMAGE_HH

#include "CassetteImage.hh"
#include "WavData.hh"
#include "DynamicClock.hh"
#include "noncopyable.hh"

namespace openmsx {

class Filename;
class FilePool;

class WavImage final : public CassetteImage, private noncopyable
{
public:
	explicit WavImage(const Filename& filename, FilePool& filePool);
	virtual ~WavImage();

	virtual short getSampleAt(EmuTime::param time);
	virtual EmuTime getEndTime() const;
	virtual unsigned getFrequency() const;
	virtual void fillBuffer(unsigned pos, int** bufs, unsigned num) const;

private:
	int getSample(unsigned pos) const;

	WavData wav;
	DynamicClock clock;
	short average;
};

} // namespace openmsx

#endif
