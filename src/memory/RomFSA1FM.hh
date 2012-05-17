// $Id$

#ifndef ROMFSA1FM1_HH
#define ROMFSA1FM1_HH

#include "MSXRom.hh"
#include "RomBlocks.hh"

namespace openmsx {

class MSXMotherBoard;
class FirmwareSwitch;
class SRAM;

class RomFSA1FMSram
{
protected:
	explicit RomFSA1FMSram(const DeviceConfig& config);
	~RomFSA1FMSram();

	MSXMotherBoard& motherBoard;
	SRAM* fsSram;
};

class RomFSA1FM1 : public MSXRom, private RomFSA1FMSram
{
public:
	RomFSA1FM1(const DeviceConfig& config, std::auto_ptr<Rom> rom);
	virtual ~RomFSA1FM1();

	virtual void reset(EmuTime::param time);
	virtual byte peekMem(word address, EmuTime::param time) const;
	virtual byte readMem(word address, EmuTime::param time);
	virtual const byte* getReadCacheLine(word address) const;
	virtual void writeMem(word address, byte value,
	                      EmuTime::param time);
	virtual byte* getWriteCacheLine(word address) const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	const std::auto_ptr<FirmwareSwitch> firmwareSwitch;
};

class RomFSA1FM2 : public Rom8kBBlocks, private RomFSA1FMSram
{
public:
	RomFSA1FM2(const DeviceConfig& config, std::auto_ptr<Rom> rom);
	virtual ~RomFSA1FM2();

	virtual void reset(EmuTime::param time);
	virtual byte peekMem(word address, EmuTime::param time) const;
	virtual byte readMem(word address, EmuTime::param time);
	virtual const byte* getReadCacheLine(word address) const;
	virtual void writeMem(word address, byte value,
	                      EmuTime::param time);
	virtual byte* getWriteCacheLine(word address) const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void changeBank(byte region, byte bank);

	byte bankSelect[8];
	bool isRam[8];
	bool isEmpty[8];
	byte control;
};

} // namespace openmsx

#endif
