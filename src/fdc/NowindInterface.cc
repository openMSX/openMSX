// $Id$

#include "NowindInterface.hh"
#include "NowindHost.hh"
#include "NowindCommand.hh"
#include "Rom.hh"
#include "AmdFlash.hh"
#include "DiskChanger.hh"
#include "Clock.hh"
#include "MSXMotherBoard.hh"
#include "MSXException.hh"
#include "serialize.hh"
#include "serialize_stl.hh"
#include "ref.hh"
#include <bitset>
#include <cassert>

using std::string;

namespace openmsx {

static const unsigned MAX_NOWINDS = 8; // a-h
typedef std::bitset<MAX_NOWINDS> NowindsInUse;

NowindInterface::NowindInterface(MSXMotherBoard& motherBoard, const XMLElement& config)
	: MSXDevice(motherBoard, config)
	, rom(new Rom(motherBoard, getName() + " ROM", "rom", config))
	, flash(new AmdFlash(
		motherBoard, *rom, 16, rom->getSize() / (1024 * 64), 0, config))
	, host(new NowindHost(drives))
{
	MSXMotherBoard::SharedStuff& info =
		motherBoard.getSharedStuff("nowindsInUse");
	if (info.counter == 0) {
		assert(info.stuff == NULL);
		info.stuff = new NowindsInUse();
	}
	++info.counter;
	NowindsInUse& nowindsInUse = *reinterpret_cast<NowindsInUse*>(info.stuff);

	unsigned i = 0;
	while (nowindsInUse[i]) {
		if (++i == MAX_NOWINDS) {
			throw MSXException("Too many nowind interfaces.");
		}
	}
	nowindsInUse[i] = true;
	basename = string("nowind") + char('a' + i);

	command.reset(new NowindCommand(
		basename, motherBoard.getCommandController(), *this));

	// start with one (empty) drive
	DiskChanger* drive = command->createDiskChanger(basename, 0, motherBoard);
	drive->createCommand();
	drives.push_back(drive);

	reset(EmuTime::dummy());
}

NowindInterface::~NowindInterface()
{
	deleteDrives();

	MSXMotherBoard::SharedStuff& info =
		getMotherBoard().getSharedStuff("nowindsInUse");
	assert(info.counter);
	assert(info.stuff);
	NowindsInUse& nowindsInUse = *reinterpret_cast<NowindsInUse*>(info.stuff);

	unsigned i = basename[6] - 'a';
	assert(nowindsInUse[i]);
	nowindsInUse[i] = false;

	--info.counter;
	if (info.counter == 0) {
		assert(nowindsInUse.none());
		delete &nowindsInUse;
		info.stuff = NULL;
	}
}

void NowindInterface::deleteDrives()
{
	for (Drives::const_iterator it = drives.begin();
	     it != drives.end(); ++it) {
		delete *it;
	}
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
		static const Clock<1000> clock(EmuTime::zero);
		host->write(value, clock.getTicksTill(time));
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
	if (ar.isLoader()) {
		deleteDrives();
	}

	ar.template serializeBase<MSXDevice>(*this);
	ar.serialize("flash", *flash);
	ar.serializeWithID("drives", drives, ref(getMotherBoard()));
	ar.serialize("nowindhost", *host);
	ar.serialize("bank", bank);

	// don't serialize command, rom, basename
}
INSTANTIATE_SERIALIZE_METHODS(NowindInterface);
REGISTER_MSXDEVICE(NowindInterface, "NowindInterface");

} // namespace openmsx
