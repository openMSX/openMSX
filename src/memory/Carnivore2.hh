#ifndef CARNIVORE2_HH
#define CARNIVORE2_HH

#include "AmdFlash.hh"
#include "EEPROM_93C46.hh"
#include "MSXMapperIO.hh"
#include "Ram.hh"

#include "AY8910.hh"
#include "MSXCPUInterface.hh"
#include "MSXDevice.hh"
#include "SCC.hh"
#include "YM2413.hh"
#include "serialize_meta.hh"

#include "Math.hh"

#include <array>
#include <cstdint>
#include <utility>

namespace openmsx {

class IDEDevice;

class Carnivore2 final
	: public MSXDevice
	, public MSXMapperIOClient
	, public GlobalReadClient<Carnivore2, CT_Interval<0x0000>, CT_Interval<0x4000, 0x4010>>
{
public:
	explicit Carnivore2(DeviceConfig& config);
	~Carnivore2() override;

	void powerUp(EmuTime time) override;
	void reset(EmuTime time) override;

	[[nodiscard]] byte readMem(uint16_t address, EmuTime time) override;
	[[nodiscard]] byte peekMem(uint16_t address, EmuTime time) const override;
	void writeMem(uint16_t address, byte value, EmuTime time) override;
	void globalRead(uint16_t address, EmuTime time) override;

	[[nodiscard]] byte readIO(uint16_t port, EmuTime time) override;
	[[nodiscard]] byte peekIO(uint16_t port, EmuTime time) const override;
	void writeIO(uint16_t port, byte value, EmuTime time) override;
	[[nodiscard]] byte getSelectedSegment(byte page) const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// config regs
	[[nodiscard]] unsigned getDirectFlashAddr() const;
	[[nodiscard]] byte peekConfigRegister(uint16_t address, EmuTime time) const;
	[[nodiscard]] byte readConfigRegister(uint16_t address, EmuTime time);
	void writeSndLVL(byte value, EmuTime time);
	void writeCfgEEPR(byte value, EmuTime time);
	void writePSGCtrl(byte value, EmuTime time);
	void writePSGAlt(byte value);
	void writePFXN(byte value);
	void writeConfigRegister(uint16_t address, byte value, EmuTime time);

	[[nodiscard]] bool sccEnabled()        const { return configRegs[0x00] & 0x10; }
	[[nodiscard]] bool delayedConfig()     const { return configRegs[0x00] & 0x08; }
	[[nodiscard]] bool delayedConfig4000() const { return configRegs[0x00] & 0x04; }
	[[nodiscard]] bool readBIOSfromRAM()   const { return configRegs[0x00] & 0x02; }

	[[nodiscard]] bool slotExpanded() const { return configRegs[0x1e] & 0x80; }
	[[nodiscard]] bool memMapReadEnabled() const {
		return (configRegs[0x1e] & 0x40) && !(port3C & 0x20);
	}
	[[nodiscard]] bool fmPacPortEnabled2() const { return configRegs[0x1e] & 0x20; }
	[[nodiscard]] bool subSlotEnabled(unsigned slot) const {
		assert(slot < 4);
		return configRegs[0x1e] & (1 << slot);
	}
	[[nodiscard]] bool writePort3cEnabled() const {
		return (configRegs[0x1e] & 0x10) && !(port3C & 0x20);
	}

	enum class SubDevice : uint8_t { MultiMapper, IDE, MemoryMapper, FmPac, Nothing };

	[[nodiscard]] SubDevice getSubDevice(uint16_t address) const;

	// multi-mapper
	[[nodiscard]] bool isConfigReg(uint16_t address) const;
	[[nodiscard]] std::pair<unsigned, byte> decodeMultiMapper(uint16_t address) const;
	[[nodiscard]] bool sccAccess(uint16_t address) const;
	[[nodiscard]] byte readMultiMapperSlot(uint16_t address, EmuTime time);
	[[nodiscard]] byte peekMultiMapperSlot(uint16_t address, EmuTime time) const;
	void writeMultiMapperSlot(uint16_t address, byte value, EmuTime time);

	// IDE
	[[nodiscard]] byte readIDESlot(uint16_t address, EmuTime time);
	[[nodiscard]] byte peekIDESlot(uint16_t address, EmuTime time) const;
	void writeIDESlot(uint16_t address, byte value, EmuTime time);
	[[nodiscard]] uint16_t ideReadData(EmuTime time);
	void ideWriteData(uint16_t value, EmuTime time);
	[[nodiscard]] byte ideReadReg(byte reg, EmuTime time);
	void ideWriteReg(byte reg, byte value, EmuTime time);
	[[nodiscard]] bool ideRegsEnabled() const { return ideControlReg & 0x01; }
	[[nodiscard]] byte ideBank() const { return Math::reverseByte(ideControlReg & 0xe0); }

	// memory mapper
	[[nodiscard]] bool isMemMapControl(uint16_t address) const;
	[[nodiscard]] unsigned getMemoryMapperAddress(uint16_t address) const;
	[[nodiscard]] bool isMemoryMapperWriteProtected(uint16_t address) const;
	[[nodiscard]] byte peekMemoryMapperSlot(uint16_t address) const;
	[[nodiscard]] byte readMemoryMapperSlot(uint16_t address) const;
	void writeMemoryMapperSlot(uint16_t address, byte value);

	// fm-pac
	[[nodiscard]] byte readFmPacSlot(uint16_t address, EmuTime time);
	[[nodiscard]] byte peekFmPacSlot(uint16_t address, EmuTime time) const;
	void writeFmPacSlot(uint16_t address, byte value, EmuTime time);
	[[nodiscard]] bool fmPacPortEnabled1() const { return fmPacEnable & 0x01; }
	[[nodiscard]] bool fmPacSramEnabled() const {
		return (fmPac5ffe == 0x4d) && (fmPac5fff == 0x69);
	}
	// User-defined ID and control port I/O
	[[nodiscard]] byte idControlPort() const { return configRegs[0x35]; }

private:
	AmdFlash flash; // 8MB
	Ram ram; // 2MB
	EEPROM_93C46 eeprom;
	std::array<byte, 64> configRegs; // in reality there are less than 64
	std::array<byte, 64> shadowConfigRegs; // only regs[0x05..0x1e] have a shadow counterpart
	byte subSlotReg;
	byte port3C;

	// multi-mapper
	SCC scc;
	byte sccMode;
	std::array<byte, 4> sccBank; // mostly write-only, except to test for scc-bank

	// PSG
	AY8910 psg;
	byte psgLatch;

	// ide
	std::array<std::unique_ptr<IDEDevice>, 2> ideDevices;
	byte ideSoftReset;
	byte ideSelectedDevice;
	byte ideControlReg;
	byte ideRead;
	byte ideWrite;

	// memory-mapper
	std::array<byte, 4> memMapRegs; // only stores 6 lower bits

	// fm-pac
	YM2413 ym2413;
	byte fmPacEnable; // enable
	byte fmPacBank; // bank
	byte fmPac5ffe;
	byte fmPac5fff;

	// User-defined ID and control port I/O
	byte PF0_RV;
};
SERIALIZE_CLASS_VERSION(Carnivore2, 3);

} // namespace openmsx

#endif
