// $Id$

#ifndef __ROMFSA1FM1_HH__
#define __ROMFSA1FM1_HH__

#include "MSXRom.hh"
#include "Rom8kBBlocks.hh"
#include "SRAM.hh"
#include "FrontSwitch.hh"

namespace openmsx {

class FSA1FMRam
{
public:
	static byte* getSRAM(Config* config);

private:
	FSA1FMRam(Config* config);
	~FSA1FMRam();

	SRAM sram;
};

class RomFSA1FM1 : public MSXRom
{
public:
	RomFSA1FM1(Config* config, const EmuTime& time, auto_ptr<Rom> rom);
	virtual ~RomFSA1FM1();
	
	virtual void reset(const EmuTime& time);
	virtual byte readMem(word address, const EmuTime& time);
	virtual const byte* getReadCacheLine(word address) const;
	virtual void writeMem(word address, byte value,
			      const EmuTime& time);
	virtual byte* getWriteCacheLine(word address) const;

private:
	byte* sram;	// 8kb (shared) sram
	FrontSwitch frontSwitch;
};

class RomFSA1FM2 : public Rom8kBBlocks
{
public:
	RomFSA1FM2(Config* config, const EmuTime& time, auto_ptr<Rom> rom);
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
	byte* sram;	// 8kb (shared) sram
	byte bankSelect[8];
	bool isRam[8];
	bool isEmpty[8];
};

} // namespace openmsx

#endif
