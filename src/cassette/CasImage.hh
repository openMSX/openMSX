#ifndef CASIMAGE_HH
#define CASIMAGE_HH

#include "CassetteImage.hh"
#include "openmsx.hh"
#include "span.hh"
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
	int16_t getSampleAt(EmuTime::param time) override;
	[[nodiscard]] EmuTime getEndTime() const override;
	[[nodiscard]] unsigned getFrequency() const override;
	void fillBuffer(unsigned pos, float** bufs, unsigned num) const override;
	[[nodiscard]] float getAmplificationFactorImpl() const override;

private:
	void write0();
	void write1();
	void writeHeader(unsigned s);
	void writeSilence(unsigned s);
	void writeByte(byte b);
	bool writeData(span<const byte> buf, size_t& pos);
	void convert(const Filename& filename, FilePool& filePool, CliComm& cliComm);

	std::vector<signed char> output;
};

} // namespace openmsx

#endif
