#ifndef WAVIMAGE_HH
#define WAVIMAGE_HH

#include "CassetteImage.hh"
#include "WavData.hh"
#include "DynamicClock.hh"
#include <cstdint>

namespace openmsx {

class Filename;
class FilePool;

class WavImage final : public CassetteImage
{
public:
	explicit WavImage(const Filename& filename, FilePool& filePool);

	int16_t getSampleAt(EmuTime::param time) override;
	bool cassetteIn(EmuTime::param time) override;
	EmuTime getEndTime() const override;
	unsigned getFrequency() const override;
	void fillBuffer(unsigned pos, int** bufs, unsigned num) const override;

private:
	int16_t getSample(unsigned pos) const;
	bool cassetteIn(int pos);

	WavData wav;
	DynamicClock clock;
	int schmittPos;
	bool schmittVal;
};

} // namespace openmsx

#endif
