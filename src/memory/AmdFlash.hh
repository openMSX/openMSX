// $Id$

#ifndef AMDFLASH_HH
#define AMDFLASH_HH

#include "openmsx.hh"
#include "noncopyable.hh"
#include <vector>
#include <memory>

namespace openmsx {

class Rom;
class SRAM;
class XMLElement;

class AmdFlash : private noncopyable
{
public:
	AmdFlash(std::auto_ptr<Rom> rom, unsigned logSectorSize,
	         unsigned writeProtectedFlags, const XMLElement& config);
	~AmdFlash();

	void reset();

	unsigned getSize() const;
	byte read(unsigned address);
	byte peek(unsigned address) const;
	void write(unsigned address, byte value);
	const byte* getReadCacheLine(unsigned address) const;

private:
	bool checkCommandEraseSector();
	bool checkCommandEraseChip();
	bool checkCommandProgram();
	bool checkCommandManifacturer();
	bool partialMatch(unsigned len, const byte* dataSeq) const;

	std::auto_ptr<Rom> rom;
	std::auto_ptr<SRAM> ram;
	const unsigned logSectorSize;
	const unsigned sectorMask;
	std::vector<int> writeAddress;
	std::vector<const byte*> readAddress;
	unsigned cmdAddr[2];

	static const unsigned MAX_CMD_SIZE = 8;
	struct AmdCmd {
		unsigned addr;
		byte value;
	} cmd[MAX_CMD_SIZE];
	unsigned cmdIdx;
	enum { ST_IDLE, ST_IDENT } state;
};

} // namespace openmsx

#endif

