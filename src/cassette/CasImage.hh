#ifndef CASIMAGE_HH
#define CASIMAGE_HH

#include "CassetteImage.hh"
#include <cstdint>
#include <vector>

namespace openmsx {

class CliComm;
class Filename;
class FilePool;

/**
 * Code based on "cas2wav" tool by Vincent van Dam
 */
class CasImage final : public CassetteImage
{
public:
	CasImage(const Filename& fileName, FilePool& filePool, CliComm& cliComm);

	// CassetteImage
	int16_t getSampleAt(EmuTime::param time) const override;
	[[nodiscard]] EmuTime getEndTime() const override;
	[[nodiscard]] unsigned getFrequency() const override;
	void fillBuffer(unsigned pos, std::span<float*, 1> bufs, unsigned num) const override;
	[[nodiscard]] float getAmplificationFactorImpl() const override;

	struct Data {
		std::vector<int8_t> wave;
		unsigned frequency;
	};

private:
	Data init(const Filename& filename, FilePool& filePool, CliComm& cliComm);

private:
	const Data data;
};

} // namespace openmsx

#endif
