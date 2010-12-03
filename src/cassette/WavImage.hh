// $Id$

#ifndef WAVIMAGE_HH
#define WAVIMAGE_HH

#include "CassetteImage.hh"
#include "DynamicClock.hh"
#include "noncopyable.hh"
#include <memory>

namespace openmsx {

class Filename;
class FilePool;
class WavData;

class WavImage : public CassetteImage, private noncopyable
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

	std::auto_ptr<WavData> wav;
	DynamicClock clock;
	short average;
};

} // namespace openmsx

#endif
