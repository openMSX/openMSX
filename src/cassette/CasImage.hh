// $Id$

#ifndef CASIMAGE_HH
#define CASIMAGE_HH

#include "CassetteImage.hh"
#include "openmsx.hh"
#include <string>
#include <vector>

namespace openmsx {

/**
 * Code based on "cas2wav" tool by Vincent van Dam
 */
class CasImage : public CassetteImage
{
public:
	CasImage(const std::string& fileName);
	virtual ~CasImage();

	virtual short getSampleAt(const EmuTime& time);

private:
	void write0();
	void write1();
	void writeHeader(int s);
	void writeSilence(int s);
	void writeByte(byte b);
	bool writeData();
	void convert();

	unsigned pos, size;
	byte* buf;
	std::vector<char> output;
};

} // namespace openmsx

#endif
