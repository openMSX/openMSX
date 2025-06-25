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
	WavImage(const WavImage&) = delete;
	WavImage(WavImage&&) = delete;
	WavImage& operator=(const WavImage&) = delete;
	WavImage& operator=(WavImage&&) = delete;
	~WavImage() override;

	[[nodiscard]] int16_t getSampleAt(EmuTime time) const override;
	[[nodiscard]] EmuTime getEndTime() const override;
	[[nodiscard]] unsigned getFrequency() const override;
	void fillBuffer(unsigned pos, std::span<float*, 1> bufs, unsigned num) const override;
	[[nodiscard]] float getAmplificationFactorImpl() const override;

private:
	const WavData* wav;
	DynamicClock clock{EmuTime::zero()};
};

} // namespace openmsx

#endif
