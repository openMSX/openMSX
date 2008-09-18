// $Id$

#ifndef ROMFSA1FM1_HH
#define ROMFSA1FM1_HH

#include "MSXRom.hh"
#include "RomBlocks.hh"

namespace openmsx {

class MSXMotherBoard;
class FirmwareSwitch;
class SRAM;

class RomFSA1FM1 : public MSXRom
{
public:
	RomFSA1FM1(MSXMotherBoard& motherBoard, const XMLElement& config,
	           std::auto_ptr<Rom> rom);
	virtual ~RomFSA1FM1();

	virtual void reset(const EmuTime& time);
	virtual byte peekMem(word address, const EmuTime& time) const;
	virtual byte readMem(word address, const EmuTime& time);
	virtual const byte* getReadCacheLine(word address) const;
	virtual void writeMem(word address, byte value,
	                      const EmuTime& time);
	virtual byte* getWriteCacheLine(word address) const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	const std::auto_ptr<FirmwareSwitch> firmwareSwitch;
	SRAM& sram;
};

class RomFSA1FM2 : public Rom8kBBlocks
{
public:
	RomFSA1FM2(MSXMotherBoard& motherBoard, const XMLElement& config,
	           std::auto_ptr<Rom> rom);
	virtual ~RomFSA1FM2();

	virtual void reset(const EmuTime& time);
	virtual byte peekMem(word address, const EmuTime& time) const;
	virtual byte readMem(word address, const EmuTime& time);
	virtual const byte* getReadCacheLine(word address) const;
	virtual void writeMem(word address, byte value,
	                      const EmuTime& time);
	virtual byte* getWriteCacheLine(word address) const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void changeBank(byte region, byte bank);

	SRAM& sram;
	byte bankSelect[8];
	bool isRam[8];
	bool isEmpty[8];
	byte control;
};

} // namespace openmsx

#endif
