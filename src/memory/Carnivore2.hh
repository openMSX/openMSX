#ifndef CARNIVORE2_HH
#define CARNIVORE2_HH

#include "MSXDevice.hh"
#include "MSXMapperIO.hh"
#include "MSXCPUInterface.hh"
#include "AmdFlash.hh"
#include "EEPROM_93C46.hh"
#include "Math.hh"
#include "Ram.hh"
#include "SCC.hh"
#include "YM2413.hh"
#include "serialize_meta.hh"
#include <utility>

namespace openmsx {

class IDEDevice;

class Carnivore2 final
	: public MSXDevice
	, public MSXMapperIOClient
	, public GlobalReadClient<Carnivore2, CT_Interval<0x0000>, CT_Interval<0x4000, 0x4010>>
{
public:
	Carnivore2(const DeviceConfig& config);
	~Carnivore2() override;

	void powerUp(EmuTime::param time) override;
	void reset(EmuTime::param time) override;

	byte readMem(word address, EmuTime::param time) override;
	byte peekMem(word address, EmuTime::param time) const override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	void globalRead(word address, EmuTime::param time) override;

	byte readIO(word port, EmuTime::param time) override;
	byte peekIO(word port, EmuTime::param time) const override;
	void writeIO(word port, byte value, EmuTime::param time) override;
	byte getSelectedSegment(byte page) const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// config regs
	unsigned getDirectFlashAddr() const;
	byte peekConfigRegister(word address, EmuTime::param time) const;
	byte readConfigRegister(word address, EmuTime::param time);
	void writeSndLVL(byte value, EmuTime::param time);
	void writeCfgEEPR(byte value, EmuTime::param time);
	void writeConfigRegister(word address, byte value, EmuTime::param time);

	bool sccEnabled()        const { return configRegs[0x00] & 0x10; }
	bool delayedConfig()     const { return configRegs[0x00] & 0x08; }
	bool delayedConfig4000() const { return configRegs[0x00] & 0x04; }
	bool readBIOSfromRAM()   const { return configRegs[0x00] & 0x02; }

	bool slotExpanded() const { return configRegs[0x1e] & 0x80; }
	bool memMapReadEnabled() const {
		return (configRegs[0x1e] & 0x40) && !(port3C & 0x20);
	}
	bool fmPacPortEnabled2() const { return configRegs[0x1e] & 0x20; }
	bool subSlotEnabled(unsigned slot) const {
		assert(slot < 4);
		return configRegs[0x1e] & (1 << slot);
	}
	bool writePort3cEnabled() const {
		return (configRegs[0x1e] & 0x10) && !(port3C & 0x20);
	}

	byte getSubSlot(word address) const;

	// multi-mapper
	bool isConfigReg(word address) const;
	std::pair<unsigned, byte> decodeMultiMapper(word address) const;
	bool sccAccess(word address) const;
	byte readMultiMapperSlot(word address, EmuTime::param time);
	byte peekMultiMapperSlot(word address, EmuTime::param time) const;
	void writeMultiMapperSlot(word address, byte value, EmuTime::param time);

	// IDE
	byte readIDESlot(word address, EmuTime::param time);
	byte peekIDESlot(word address, EmuTime::param time) const;
	void writeIDESlot(word address, byte value, EmuTime::param time);
	word ideReadData(EmuTime::param time);
	void ideWriteData(word value, EmuTime::param time);
	byte ideReadReg(byte reg, EmuTime::param time);
	void ideWriteReg(byte reg, byte value, EmuTime::param time);
	bool ideRegsEnabled() const { return ideControlReg & 0x01; }
	byte ideBank() const { return Math::reverseByte(ideControlReg & 0xe0); }

	// memory mapper
	bool isMemmapControl(word address) const;
	unsigned getMemoryMapperAddress(word address) const;
	bool isMemoryMapperWriteProtected(word address) const;
	byte peekMemoryMapperSlot(word address) const;
	byte readMemoryMapperSlot(word address);
	void writeMemoryMapperSlot(word address, byte value);

	// fm-pac
	byte readFmPacSlot(word address);
	byte peekFmPacSlot(word address) const;
	void writeFmPacSlot(word address, byte value, EmuTime::param time);
	bool fmPacPortEnabled1() const { return fmPacEnable & 0x01; }
	bool fmPacSramEnabled() const {
		return (fmPac5ffe == 0x4d) && (fmPac5fff == 0x69);
	}

private:
	AmdFlash flash; // 8MB
	Ram ram; // 2MB
	EEPROM_93C46 eeprom;
	byte configRegs[64]; // in reality there are less than 64
	byte shadowConfigRegs[64]; // only regs[0x05..0x1e] have a shadow counterpart
	byte subSlotReg;
	byte port3C;

	// multi-mapper
	SCC scc;
	byte sccMode;
	byte sccBank[4]; // mostly write-only, except to test for scc-bank

	// ide
	std::unique_ptr<IDEDevice> ideDevices[2];
	byte ideSoftReset;
	byte ideSelectedDevice;
	byte ideControlReg;
	byte ideRead;
	byte ideWrite;

	// memory-mapper
	byte memMapRegs[4]; // only stores 6 lower bits
	
	// fm-pac
	YM2413 ym2413;
	byte fmPacEnable; // enable
	byte fmPacBank; // bank
	byte fmPac5ffe;
	byte fmPac5fff;
};
SERIALIZE_CLASS_VERSION(Carnivore2, 2);

} // namespace openmsx

#endif
