#include "ColecoSuperGameModule.hh"
#include "DummyAY8910Periphery.hh"
#include "MSXCPUInterface.hh"
#include "MSXException.hh"
#include "serialize.hh"

namespace openmsx {

// Disabling the SGM RAM has no effect on 0-0x1FFF, according to Oscar Toledo.
// So, if the BIOS is disabled to show RAM and the SGM RAM is disabled, is
// there is 8kB SGM RAM on 0-0x1FFF.

constexpr unsigned MAIN_RAM_AREA_START = 0x6000;
constexpr unsigned MAIN_RAM_SIZE = 0x400; // 1kB
constexpr unsigned SGM_RAM_SIZE = 0x8000; // 32kB
constexpr unsigned BIOS_ROM_SIZE = 0x2000; // 8kB

ColecoSuperGameModule::ColecoSuperGameModule(const DeviceConfig& config)
	: MSXDevice(config)
	, psg(getName() + " PSG", DummyAY8910Periphery::instance(), config, getCurrentTime())
	, sgmRam(config, getName() + " RAM", "SGM RAM", SGM_RAM_SIZE)
	, mainRam(config, "Main RAM", "Main RAM", MAIN_RAM_SIZE)
	, biosRom(getName(), "BIOS ROM", config)
{
	if (biosRom.getSize() != BIOS_ROM_SIZE) {
		throw MSXException("ColecoVision BIOS ROM must be exactly 8kB in size.");
	}
	getCPUInterface().register_IO_Out(0x50, this);
	getCPUInterface().register_IO_Out(0x51, this);
	getCPUInterface().register_IO_In (0x52, this);
	getCPUInterface().register_IO_Out(0x53, this);
	getCPUInterface().register_IO_Out(0x7F, this);
	reset(getCurrentTime());
}

ColecoSuperGameModule::~ColecoSuperGameModule()
{
	getCPUInterface().unregister_IO_Out(0x50, this);
	getCPUInterface().unregister_IO_Out(0x51, this);
	getCPUInterface().unregister_IO_In (0x52, this);
	getCPUInterface().unregister_IO_Out(0x53, this);
	getCPUInterface().unregister_IO_Out(0x7F, this);
}

static constexpr unsigned translateMainRamAddress(unsigned address)
{
	return address & (MAIN_RAM_SIZE - 1);
}

void ColecoSuperGameModule::reset(EmuTime::param time)
{
	ramEnabled = false;
	ramAtBiosEnabled = false;
	psgLatch = 0;
	psg.reset(time);
	invalidateDeviceRWCache(); // flush all to be sure
}

byte ColecoSuperGameModule::readIO(word port, EmuTime::param time)
{
	if ((port & 0xFF) == 0x52) {
		return psg.readRegister(psgLatch, time);
	}
	return 0xFF;
}

byte ColecoSuperGameModule::peekIO(word port, EmuTime::param time) const
{
	if ((port & 0xFF) == 0x52) {
		return psg.peekRegister(psgLatch, time);
	}
	return 0xFF;
}

void ColecoSuperGameModule::writeIO(word port, byte value, EmuTime::param time)
{
	switch (port & 0xFF) {
		case 0x50: // PSG address (latch?)
			psgLatch = value & 0x0F;
			break;
		case 0x51: // PSG data (register write?)
			psg.writeRegister(psgLatch, value, time);
			break;
		case 0x53: // bit0=1 means enable SGM RAM in 0x2000-0x7FFF range
			ramEnabled = (value & 1) != 0;
			invalidateDeviceRWCache(0x0000, SGM_RAM_SIZE); // just flush the whole area
			break;
		case 0x7F: // bit1=0 means enable SGM RAM in BIOS area (0-0x1FFF), 1 means BIOS
			ramAtBiosEnabled = (value & 2) == 0;
			invalidateDeviceRWCache(0x0000, BIOS_ROM_SIZE);
			break;
		default:
			// ignore
			break;
	}
}

byte ColecoSuperGameModule::peekMem(word address, EmuTime::param /*time*/) const
{
	if (address < BIOS_ROM_SIZE) {
		return ramAtBiosEnabled ? sgmRam.peek(address) : biosRom[address];
	} else if (address < SGM_RAM_SIZE) {
		if (ramEnabled) {
			return sgmRam.peek(address);
		} else if (address >= MAIN_RAM_AREA_START) {
			return mainRam.peek(translateMainRamAddress(address));
		}
	}
	return 0xFF;
}

byte ColecoSuperGameModule::readMem(word address, EmuTime::param /*time*/)
{
	if (address < BIOS_ROM_SIZE) {
		return ramAtBiosEnabled ? sgmRam.read(address) : biosRom[address];
	} else if (address < SGM_RAM_SIZE) {
		if (ramEnabled) {
			return sgmRam.read(address);
		} else if (address >= MAIN_RAM_AREA_START) {
			return mainRam.read(translateMainRamAddress(address));
		}
	}
	return 0xFF;
}

void ColecoSuperGameModule::writeMem(word address, byte value, EmuTime::param /*time*/)
{
	if (address < BIOS_ROM_SIZE) {
		if (ramAtBiosEnabled) {
			sgmRam.write(address, value);
		}
	} else if (address < SGM_RAM_SIZE) {
		if (ramEnabled) {
			sgmRam.write(address, value);
		} else if (address >= MAIN_RAM_AREA_START) {
			mainRam.write(translateMainRamAddress(address), value);
		}
	}
}

const byte* ColecoSuperGameModule::getReadCacheLine(word start) const
{
	if (start < BIOS_ROM_SIZE) {
		return ramAtBiosEnabled ? sgmRam.getReadCacheLine(start) : &biosRom[start];
	} else if (start < SGM_RAM_SIZE) {
		if (ramEnabled) {
			return sgmRam.getReadCacheLine(start);
		} else if (start >= MAIN_RAM_AREA_START) {
			return mainRam.getReadCacheLine(translateMainRamAddress(start));
		}
	}
	return unmappedRead;
}

byte* ColecoSuperGameModule::getWriteCacheLine(word start) const
{
	if (start < BIOS_ROM_SIZE) {
		if (ramAtBiosEnabled) {
			return sgmRam.getWriteCacheLine(start);
		}
	} else if (start < SGM_RAM_SIZE) {
		if (ramEnabled) {
			return sgmRam.getWriteCacheLine(start);
		} else if (start >= MAIN_RAM_AREA_START) {
			return mainRam.getWriteCacheLine(translateMainRamAddress(start));
		}
	}
	return unmappedWrite;
}

template<typename Archive>
void ColecoSuperGameModule::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("mainRam",          mainRam.getUncheckedRam(),
	             "sgmRam",           sgmRam.getUncheckedRam(),
	             "psg",              psg,
	             "psgLatch",         psgLatch,
	             "ramEnabled",       ramEnabled,
	             "ramAtBiosEnabled", ramAtBiosEnabled);
}
INSTANTIATE_SERIALIZE_METHODS(ColecoSuperGameModule);
REGISTER_MSXDEVICE(ColecoSuperGameModule, "ColecoSuperGameModule");

} // namespace openmsx
