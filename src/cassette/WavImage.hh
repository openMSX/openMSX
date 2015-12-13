#ifndef WAVIMAGE_HH
#define WAVIMAGE_HH

#include "CassetteImage.hh"
#include "WavData.hh"
#include "DynamicClock.hh"

namespace openmsx {

class Filename;
class FilePool;

class WavImage final : public CassetteImage
{
public:
	explicit WavImage(const Filename& filename, FilePool& filePool);
	~WavImage();

	short getSampleAt(EmuTime::param time) override;
	EmuTime getEndTime() const override;
	unsigned getFrequency() const override;
	void fillBuffer(unsigned pos, int** bufs, unsigned num) const override;

private:
	int getSample(unsigned pos) const;

	WavData wav;
	DynamicClock clock;
	short average;
};

} // namespace openmsx

#endif
