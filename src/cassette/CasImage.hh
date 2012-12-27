// $Id$

#ifndef CASIMAGE_HH
#define CASIMAGE_HH

#include "CassetteImage.hh"
#include "openmsx.hh"
#include <vector>

namespace openmsx {

class CliComm;
class Filename;
class FilePool;

/**
 * Code based on "cas2wav" tool by Vincent van Dam
 */
class CasImage : public CassetteImage
{
public:
	CasImage(const Filename& fileName, FilePool& filePool, CliComm& cliComm);

	// CassetteImage
	virtual short getSampleAt(EmuTime::param time);
	virtual EmuTime getEndTime() const;
	virtual unsigned getFrequency() const;
	virtual void fillBuffer(unsigned pos, int** bufs, unsigned num) const;

private:
	void write0();
	void write1();
	void writeHeader(int s);
	void writeSilence(int s);
	void writeByte(byte b);
	bool writeData(const byte* buf, size_t size, size_t& pos);
	void convert(const Filename& filename, FilePool& filePool, CliComm& cliComm);

	std::vector<signed char> output;
};

} // namespace openmsx

#endif
