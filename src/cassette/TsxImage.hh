// Based on code written by:
//   (2017) NataliaPC aka @ishwin74
//   Under GPL License

#ifndef TSXIMAGE_HH
#define TSXIMAGE_HH

#include "CassetteImage.hh"
#include <vector>

namespace openmsx {

class CliComm;
class Filename;
class FilePool;

class TsxImage final : public CassetteImage
{
public:
	TsxImage(const Filename& fileName, FilePool& filePool, CliComm& cliComm);

	// CassetteImage
	[[nodiscard]] int16_t getSampleAt(EmuTime time) const override;
	[[nodiscard]] EmuTime getEndTime() const override;
	[[nodiscard]] unsigned getFrequency() const override;
	void fillBuffer(unsigned pos, std::span<float*, 1> bufs, unsigned num) const override;
	[[nodiscard]] float getAmplificationFactorImpl() const override;

private:
	/*const*/ std::vector<signed char> output;
};

} // namespace openmsx

#endif
