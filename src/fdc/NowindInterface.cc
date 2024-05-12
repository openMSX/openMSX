#include "NowindInterface.hh"
#include "DiskChanger.hh"
#include "Clock.hh"
#include "MSXMotherBoard.hh"
#include "MSXException.hh"
#include "narrow.hh"
#include "serialize.hh"
#include "serialize_stl.hh"
#include <cassert>
#include <functional>
#include <memory>

namespace openmsx {

NowindInterface::NowindInterface(const DeviceConfig& config)
	: MSXDevice(config)
	, rom(getName() + " ROM", "rom", config)
	, flashConfig(rom.size() / 0x10000, {0x10000, false})
	, flash(rom, AmdFlashChip::AM29F040, flashConfig,
	        AmdFlash::Addressing::BITS_11, config)
	, host(drives)
	, basename("nowindX")
{
	if ((rom.size() % 0x4000) != 0) {
		throw MSXException("NowindInterface ROM size must be a multiple of 16kB");
	}
	if (rom.size() == 0) {
		throw MSXException("NowindInterface ROM size cannot be zero");
	}
	if (rom.size() > size_t(256 * 0x4000)) {
		throw MSXException("NowindInterface ROM size cannot be larger than 4MB");
	}

	nowindsInUse = getMotherBoard().getSharedStuff<NowindsInUse>("nowindsInUse");

	unsigned i = 0;
	while ((*nowindsInUse)[i]) {
		if (++i == MAX_NOWINDS) {
			throw MSXException("Too many nowind interfaces.");
		}
	}
	(*nowindsInUse)[i] = true;
	basename[6] = char('a' + i);

	command.emplace(basename, getCommandController(), *this);

	// start with one (empty) drive
	auto drive = command->createDiskChanger(basename, 0, getMotherBoard());
	drive->createCommand();
	drives.push_back(std::move(drive));

	reset(EmuTime::dummy());
}

NowindInterface::~NowindInterface()
{
	unsigned i = basename[6] - 'a';
	assert((*nowindsInUse)[i]);
	(*nowindsInUse)[i] = false;
}

void NowindInterface::reset(EmuTime::param /*time*/)
{
	// version 1 didn't change the bank number
	// version 2 (produced by Sunrise) does reset the bank number
	bank = 0;

	// Flash state is NOT changed on reset
	//flash.reset();
}

byte NowindInterface::peekMem(word address, EmuTime::param /*time*/) const
{
	if (((0x2000 <= address) && (address < 0x4000)) ||
	    ((0x8000 <= address) && (address < 0xA000))) {
		return host.peek();
	} else if ((0x4000 <= address) && (address < 0xC000)) {
		// note: range 0x8000-0xA000 is already handled above
		return flash.peek(bank * 0x4000 + (address & 0x3FFF));
	} else {
		return 0xFF;
	}
}

byte NowindInterface::readMem(word address, EmuTime::param /*time*/)
{
	if (((0x2000 <= address) && (address < 0x4000)) ||
	    ((0x8000 <= address) && (address < 0xA000))) {
		return host.read();
	} else if ((0x4000 <= address) && (address < 0xC000)) {
		// note: range 0x8000-0xA000 is already handled above
		return flash.read(bank * 0x4000 + (address & 0x3FFF));
	} else {
		return 0xFF;
	}
}

const byte* NowindInterface::getReadCacheLine(word address) const
{
	if (((0x2000 <= address) && (address < 0x4000)) ||
	    ((0x8000 <= address) && (address < 0xA000))) {
		// nowind region, not cacheable
		return nullptr;
	} else if ((0x4000 <= address) && (address < 0xC000)) {
		// note: range 0x8000-0xA000 is already handled above
		return flash.getReadCacheLine(bank * 0x4000 + (address & 0x3FFF));
	} else {
		return unmappedRead.data();
	}
}

void NowindInterface::writeMem(word address, byte value, EmuTime::param time)
{
	if (address < 0x4000) {
		flash.write(bank * 0x4000 + address, value);
	} else if (((0x4000 <= address) && (address < 0x6000)) ||
	           ((0x8000 <= address) && (address < 0xA000))) {
		constexpr Clock<1000> clock(EmuTime::zero());
		host.write(value, clock.getTicksTill(time));
	} else if (((0x6000 <= address) && (address < 0x8000)) ||
	           ((0xA000 <= address) && (address < 0xC000))) {
		auto max = narrow<uint8_t>((rom.size() / 0x4000) - 1);
		bank = (value <= max) ? value : (value & max);
		invalidateDeviceRCache(0x4000, 0x4000);
		invalidateDeviceRCache(0xA000, 0x2000);
	}
}

byte* NowindInterface::getWriteCacheLine(word address)
{
	if (address < 0xC000) {
		// not cacheable
		return nullptr;
	} else {
		return unmappedWrite.data();
	}
}


template<typename Archive>
void NowindInterface::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	ar.serialize("flash", flash);
	ar.serializeWithID("drives", drives, std::ref(getMotherBoard()));
	ar.serialize("nowindhost", host,
	             "bank",       bank);

	// don't serialize command, rom, basename
}
INSTANTIATE_SERIALIZE_METHODS(NowindInterface);
REGISTER_MSXDEVICE(NowindInterface, "NowindInterface");

} // namespace openmsx
