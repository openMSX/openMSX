#include "MSXMusic.hh"
#include "CacheLine.hh"
#include "MSXException.hh"
#include "serialize.hh"
#include <bit>

namespace openmsx {

// class MSXMusicBase

MSXMusicBase::MSXMusicBase(const DeviceConfig& config)
	: MSXDevice(config)
	, rom(getName() + " ROM", "rom", config)
	, ym2413(getName(), config)
{
	if (!std::has_single_bit(rom.size())) {
		throw MSXException("MSX-Music ROM-size must be a non-zero power of two");
	}
	MSXMusicBase::reset(getCurrentTime());
}

void MSXMusicBase::reset(EmuTime::param time)
{
	ym2413.reset(time);
}

void MSXMusicBase::writeIO(word port, byte value, EmuTime::param time)
{
	writePort(port & 1, value, time);
}

void MSXMusicBase::writePort(bool port, byte value, EmuTime::param time)
{
	ym2413.writePort(port, value, time);
}

byte MSXMusicBase::peekMem(word address, EmuTime::param /*time*/) const
{
	return *MSXMusicBase::getReadCacheLine(address);
}

byte MSXMusicBase::readMem(word address, EmuTime::param time)
{
	return peekMem(address, time);
}

const byte* MSXMusicBase::getReadCacheLine(word start) const
{
	return &rom[start & (rom.size() - 1)];
}

// version 1: initial version
// version 2: refactored YM2413 class structure
// version 3: removed 'registerLatch' (moved to YM2413 cores)
template<typename Archive>
void MSXMusicBase::serialize(Archive& ar, unsigned version)
{
	ar.template serializeBase<MSXDevice>(*this);

	if (ar.versionAtLeast(version, 2)) {
		ar.serialize("ym2413", ym2413);
	} else {
		// In older versions, the 'ym2413' level was missing, delegate
		// directly to YM2413 without emitting the 'ym2413' tag.
		ym2413.serialize(ar, version);
	}
}
INSTANTIATE_SERIALIZE_METHODS(MSXMusicBase);



// class MSXMusic

MSXMusic::MSXMusic(const DeviceConfig& config)
	: MSXMusicBase(config)
{
}

template<typename Archive>
void MSXMusic::serialize(Archive& ar, unsigned version)
{
	ar.template serializeInlinedBase<MSXMusicBase>(*this, version);
}
INSTANTIATE_SERIALIZE_METHODS(MSXMusic);
REGISTER_MSXDEVICE(MSXMusic, "MSX-Music");



// class MSXMusicWX

// Thanks to NYYRIKKI for figuring this out:
//  - writes to 0x7ff0-0x7fff set a control register (mirrored 16x)
//  - writes to any other memory region have no effect
//  - bit 0 of this control register can be used to disable reading the ROM
//    (0=enable, 1=disabled), other bits seem to have no effect
//  - reading from 0x7ff0-0x7fff return the last written value OR 0xfc, IOW the
//    lower two bits are the last written value, higher bits always read 1
//  - reading from 0x4000-0x7fef returns the content of the ROM if the ROM is
//    enabled (bit 0 of the control register = 0), when the ROM is disabled
//    reads return 0xff
//  - reading any other memory location returns 0xff

MSXMusicWX::MSXMusicWX(const DeviceConfig& config)
	: MSXMusicBase(config)
{
	reset(getCurrentTime());
}

void MSXMusicWX::reset(EmuTime::param time)
{
	MSXMusicBase::reset(time);
	control = 0;
}

byte MSXMusicWX::peekMem(word address, EmuTime::param time) const
{
	if ((0x7FF0 <= address) && (address < 0x8000)) {
		return control | 0xFC;
	} else if ((control & 1) == 0) {
		return MSXMusicBase::peekMem(address, time);
	} else {
		return 0xFF;
	}
}

byte MSXMusicWX::readMem(word address, EmuTime::param time)
{
	return peekMem(address, time);
}

const byte* MSXMusicWX::getReadCacheLine(word start) const
{
	if ((0x7FF0 & CacheLine::HIGH) == start) {
		return nullptr;
	} else if ((control & 1) == 0) {
		return MSXMusicBase::getReadCacheLine(start);
	} else {
		return unmappedRead.data();
	}
}

void MSXMusicWX::writeMem(word address, byte value, EmuTime::param /*time*/)
{
	if ((0x7FF0 <= address) && (address < 0x8000)) {
		control = value & 3;
		invalidateDeviceRCache();
	}
}

byte* MSXMusicWX::getWriteCacheLine(word start) const
{
	if ((0x7FF0 & CacheLine::HIGH) == start) {
		return nullptr;
	} else {
		return unmappedWrite.data();
	}
}

template<typename Archive>
void MSXMusicWX::serialize(Archive& ar, unsigned version)
{
	ar.template serializeInlinedBase<MSXMusicBase>(*this, version);
	ar.serialize("control", control);
}
INSTANTIATE_SERIALIZE_METHODS(MSXMusicWX);
REGISTER_MSXDEVICE(MSXMusicWX, "MSX-Music-WX");

} // namespace openmsx
