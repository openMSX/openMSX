// $Id$

// TODO
// - multiple drives, including romdisk
// - allowOtherDiskroms / enablePhantomDrives
// - add 'usbhost' command to control these features (with same syntax as
//   the 'usbhost' command of the real nowind interface)

#include "NowindInterface.hh"
#include "Rom.hh"
#include "AmdFlash.hh"
#include "DiskChanger.hh"
#include "NowindHost.hh"
#include "MSXMotherBoard.hh"
#include "serialize.hh"

namespace openmsx {

NowindInterface::NowindInterface(MSXMotherBoard& motherBoard, const XMLElement& config)
	: MSXDevice(motherBoard, config)
	, rom(new Rom(motherBoard, getName() + " ROM", "rom", config))
	, flash(new AmdFlash(*rom, 16, rom->getSize() / (1024 * 64), 0, config))
	, changer(new DiskChanger("nowind",
	                          motherBoard.getCommandController(),
	                          motherBoard.getDiskManipulator(),
	                          &motherBoard))
	, host(new NowindHost(drives))
{
	drives.push_back(changer.get()); // TODO make dynamic
	reset(EmuTime::dummy());
}

NowindInterface::~NowindInterface()
{
}

void NowindInterface::reset(EmuTime::param /*time*/)
{
	// version 1 didn't change the bank number
	// version 2 (produced by Sunrise) does reset the bank number
	bank = 0;

	// Flash state is NOT changed on reset
	//flash->reset();
}

byte NowindInterface::peek(word address, EmuTime::param /*time*/) const
{
	if (((0x2000 <= address) && (address < 0x4000)) ||
	    ((0x8000 <= address) && (address < 0xA000))) {
		return host->peek();
	} else if ((0x4000 <= address) && (address < 0xC000)) {
		// note: range 0x8000-0xA000 is already handled above
		return flash->peek(bank * 0x4000 + (address & 0x3FFF));
	} else {
		return 0xFF;
	}
}

byte NowindInterface::readMem(word address, EmuTime::param /*time*/)
{
	if (((0x2000 <= address) && (address < 0x4000)) ||
	    ((0x8000 <= address) && (address < 0xA000))) {
		return host->read();
	} else if ((0x4000 <= address) && (address < 0xC000)) {
		// note: range 0x8000-0xA000 is already handled above
		return flash->read(bank * 0x4000 + (address & 0x3FFF));
	} else {
		return 0xFF;
	}
}

const byte* NowindInterface::getReadCacheLine(word address) const
{
	if (((0x2000 <= address) && (address < 0x4000)) ||
	    ((0x8000 <= address) && (address < 0xA000))) {
		// nowind region, not cachable
		return NULL;
	} else if ((0x4000 <= address) && (address < 0xC000)) {
		// note: range 0x8000-0xA000 is already handled above
		return flash->getReadCacheLine(bank * 0x4000 + (address & 0x3FFF));
	} else {
		return unmappedRead;
	}
}

void NowindInterface::writeMem(word address, byte value, EmuTime::param time)
{
	if (address < 0x4000) {
		flash->write(bank * 0x4000 + address, value);
	} else if (((0x4000 <= address) && (address < 0x6000)) ||
	           ((0x8000 <= address) && (address < 0xA000))) {
		host->write(value, time);
	} else if (((0x6000 <= address) && (address < 0x8000)) ||
	           ((0xA000 <= address) && (address < 0xC000))) {
		byte max = rom->getSize() / (16 * 1024);
		bank = (value < max) ? value : value & (max - 1);
		invalidateMemCache(0x4000, 0x4000);
		invalidateMemCache(0xA000, 0x2000);
	}
}

byte* NowindInterface::getWriteCacheLine(word address) const
{
	if (address < 0xC000) {
		// not cachable
		return NULL;
	} else {
		return unmappedWrite;
	}
}


template<typename Archive>
void NowindInterface::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	ar.serialize("flash", *flash);
	ar.serialize("changer", *changer);
	ar.serialize("nowindhost", *host);
	ar.serialize("bank", bank);
}
INSTANTIATE_SERIALIZE_METHODS(NowindInterface);
REGISTER_MSXDEVICE(NowindInterface, "NowindInterface");

} // namespace openmsx
