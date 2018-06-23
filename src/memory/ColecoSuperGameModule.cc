#include "ColecoSuperGameModule.hh"
#include "DummyAY8910Periphery.hh"
#include "MSXCPUInterface.hh"
#include "MSXException.hh"
#include "serialize.hh"

namespace openmsx {

ColecoSuperGameModule::ColecoSuperGameModule(const DeviceConfig& config)
	: MSXDevice(config)
	, sgmRam(config, getName() + " RAM", "SGM RAM", 32*1024)
	, psg(getName() + " PSG", DummyAY8910Periphery::instance(), config, getCurrentTime())
	, mainRam(config, "Main RAM", "Main RAM", 1024)
	, biosRom(getName(), "BIOS ROM", config)
{
	if (biosRom.getSize() != 0x2000) {
		throw MSXException(
				"ColecoVision BIOS ROM must be exactly 8kB in size.");
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

unsigned ColecoSuperGameModule::translateMainRamAddress(unsigned address) const
{
	const unsigned base(0x6000);
	const unsigned size(1024);
	address -= base;
	if (address >= size) address &= (size - 1);
	return address;
}

void ColecoSuperGameModule::reset(EmuTime::param time)
{
	ramEnabled = false;
	ramAtBiosEnabled = false;
	psgLatch = 0;
	psg.reset(time);
	invalidateMemCache(0x0000, 0x10000); // flush all to be sure
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
			invalidateMemCache(0x0000, 0x8000); // just flush
			break;
		case 0x7F: // bit1=0 means enable SGM RAM in BIOS area (0-0x1FFF), 1 means BIOS
			ramAtBiosEnabled = (value & 2) == 0;
			invalidateMemCache(0x0000, 0x8000); // just flush 
			break;
		default:
			// ignore
			break;
	}
}

byte ColecoSuperGameModule::peekMem(word address, EmuTime::param /*time*/) const
{
	if (address < 0x2000) {
		return ramAtBiosEnabled ? sgmRam.peek(address) : biosRom[address];
	} else if (address < 0x8000) {
		if (ramEnabled) {
			return sgmRam.peek(address);
		} else if (address >= 0x6000) {
			return mainRam.peek(translateMainRamAddress(address));
		}
	}
	return 0xFF;
}

byte ColecoSuperGameModule::readMem(word address, EmuTime::param /*time*/)
{
	if (address < 0x2000) {
		return ramAtBiosEnabled ? sgmRam.read(address) : biosRom[address];
	} else if (address < 0x8000) {
		if (ramEnabled) {
			return sgmRam.read(address);
		} else if (address >= 0x6000) {
			return mainRam.read(translateMainRamAddress(address));
		}
	}
	return 0xFF;
}

void ColecoSuperGameModule::writeMem(word address, byte value, EmuTime::param /*time*/)
{
	if ((address < 0x2000)) {
		if (ramAtBiosEnabled) {
			sgmRam.write(address, value);
		}
	} else if (address < 0x8000) {
		if (ramEnabled) {
			sgmRam.write(address, value);
		} else if (address >= 0x6000) {
			mainRam.write(translateMainRamAddress(address), value);
		}
	}
}

const byte* ColecoSuperGameModule::getReadCacheLine(word start) const
{
	if (start < 0x2000) {
		return ramAtBiosEnabled ? sgmRam.getReadCacheLine(start) : &biosRom[start];
	} else if (start < 0x8000) {
		if (ramEnabled) {
			return sgmRam.getReadCacheLine(start);
		} else if (start >= 0x6000) {
			return mainRam.getReadCacheLine(translateMainRamAddress(start));
		}
	}
	return unmappedRead;
}

byte* ColecoSuperGameModule::getWriteCacheLine(word start) const
{
	if (start < 0x2000) {
		if (ramAtBiosEnabled) {
			return sgmRam.getWriteCacheLine(start);
		}
	} else if (start < 0x8000) {
		if (ramEnabled) {
			return sgmRam.getWriteCacheLine(start);
		} else if (start >= 0x6000) {
			return mainRam.getWriteCacheLine(translateMainRamAddress(start));
		}
	}
	return unmappedWrite;
}

template<typename Archive>
void ColecoSuperGameModule::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("mainRam", mainRam.getUncheckedRam());
	ar.serialize("sgmRam", sgmRam.getUncheckedRam());
	ar.serialize("psg", psg);
	ar.serialize("psgLatch", psgLatch);
	ar.serialize("ramEnabled", ramEnabled);
	ar.serialize("ramAtBiosEnabled", ramAtBiosEnabled);
}
INSTANTIATE_SERIALIZE_METHODS(ColecoSuperGameModule);
REGISTER_MSXDEVICE(ColecoSuperGameModule, "ColecoSuperGameModule");

} // namespace openmsx
