// $Id$

#ifndef ROMFSA1FM1_HH
#define ROMFSA1FM1_HH

#include "MSXRom.hh"
#include "Rom8kBBlocks.hh"

namespace openmsx {

class MSXMotherBoard;
class FirmwareSwitch;

class RomFSA1FM1 : public MSXRom
{
public:
	RomFSA1FM1(MSXMotherBoard& motherBoard, const XMLElement& config,
	           const EmuTime& time, std::auto_ptr<Rom> rom);
	virtual ~RomFSA1FM1();

	virtual void reset(const EmuTime& time);
	virtual byte readMem(word address, const EmuTime& time);
	virtual const byte* getReadCacheLine(word address) const;
	virtual void writeMem(word address, byte value,
	                      const EmuTime& time);
	virtual byte* getWriteCacheLine(word address) const;

private:
	const std::auto_ptr<FirmwareSwitch> firmwareSwitch;
};

class RomFSA1FM2 : public Rom8kBBlocks
{
public:
	RomFSA1FM2(MSXMotherBoard& motherBoard, const XMLElement& config,
	           const EmuTime& time, std::auto_ptr<Rom> rom);
	virtual ~RomFSA1FM2();

	virtual void reset(const EmuTime& time);
	virtual byte readMem(word address, const EmuTime& time);
	virtual const byte* getReadCacheLine(word address) const;
	virtual void writeMem(word address, byte value,
	                      const EmuTime& time);
	virtual byte* getWriteCacheLine(word address) const;

private:
	void changeBank(byte region, byte bank);

	byte control;
	byte bankSelect[8];
	bool isRam[8];
	bool isEmpty[8];
};

} // namespace openmsx

#endif
