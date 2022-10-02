#include "MSXToshibaTcx200x.hh"
#include "CacheLine.hh"
#include "serialize.hh"

namespace openmsx {

// 7FFF is mapper control register
//
// x11xxxxx select SRAM in page 2 (8000H-87FFH, or perhaps mirrored in page 2), R/W
// x00xxxxx select ROM in page 2, R/W
// xxxxxx00 select ROM segment 0 in page 2 (screencopy, japanese wordprocessor), R/W
// xxxxxx01 select ROM segment 1 in page 2 (japanese wordprocessor), R/W
// xxxxxx10 select ROM segment 2 in page 2 (european wordprocessor), R/W
// xxxxxx11 select ROM segment 3 in page 2 (japanese wordprocessor), R/W
// kxxxxxxx COPY key, 0 is pressed (R)

// b6,b5 = SRAM select. Only pattern 00 and 11 are used, not sure what pattern 01 and 10 do
// Assumption that SRAM is only visible in page 2, because the ROM segment
// select is also for page 2 only

MSXToshibaTcx200x::MSXToshibaTcx200x(const DeviceConfig& config)
	: MSXDevice(config)
	, rs232Rom(getName() + " RS232C ROM", "rom", config, "rs232")
	, wordProcessorRom(getName() + " Word Processor ROM", "rom", config, "wordpro")
	, sram(getName() + " SRAM", 0x800, config)
	, copyButtonPressed(getCommandController(), "copy_button_pressed",
                "pressed status of the COPY button", false)
{
	reset(EmuTime::dummy());
}

void MSXToshibaTcx200x::reset(EmuTime::param time)
{
	copyButtonPressed.setBoolean(false);
	writeMem(0x7FFF, 0, time);
}

bool MSXToshibaTcx200x::sramEnabled() const
{
	return (controlReg & 0b0110'0000) == 0b0110'0000;
	// note: we don't know what the hardware does if only one bit is on 1
}

byte MSXToshibaTcx200x::getSelectedSegment() const
{
	return controlReg & 0b0000'0011;
}

byte MSXToshibaTcx200x::peekMem(word address, EmuTime::param /*time*/) const
{
	if (address == 0x7FFF) {
		return controlReg | ((copyButtonPressed.getBoolean() ? 0 : 1) << 7);
	} else if ((0x4000 <= address) && (address < 0x7FFF)) {
		return rs232Rom[address - 0x4000];
	} else if ((0x8000 <= address) && (address < 0xC000)) {
		if (sramEnabled()) {
			// TODO: assuming SRAM mirrors all over page 2. Not verified!
			return sram[address & (sram.size() - 1)];
		} else {
			return wordProcessorRom[(address - 0x8000) + getSelectedSegment() * 0x4000];
		}
	} else {
		return 0xFF;
	}
}

byte MSXToshibaTcx200x::readMem(word address, EmuTime::param time)
{
	return peekMem(address, time);
}

void MSXToshibaTcx200x::writeMem(word address, byte value, EmuTime::param /*time*/)
{
	if (address == 0x7FFF) {
		controlReg = value & 0b0110'0011; // TODO which bits can be read back?
		invalidateDeviceRWCache(0x8000, 0x4000);
	} else if ((0x8000 <= address) && (address < 0xC000) && sramEnabled()) {
		sram.write(address & (sram.size() - 1), value);
	}
}

const byte* MSXToshibaTcx200x::getReadCacheLine(word start) const
{
	if ((start & CacheLine::HIGH) == (0x7FFF & CacheLine::HIGH)) {
		return nullptr;
	} else if ((0x4000 <= start) && (start < 0x7FFF)) {
		return &rs232Rom[start - 0x4000];
	} else if ((0x8000 <= start) && (start < 0xC000)) {
		if (sramEnabled()) {
			return &sram[start & (sram.size() - 1)];
		} else {
			return &wordProcessorRom[(start - 0x8000) + getSelectedSegment() * 0x4000];
		}
	}
	return unmappedRead.data();
}

byte* MSXToshibaTcx200x::getWriteCacheLine(word start) const
{
	if ((start & CacheLine::HIGH) == (0x7FFF & CacheLine::HIGH)) {
		return nullptr;
	} else if ((0x8000 <= start) && (start < 0xC000) && sramEnabled()) {
		return nullptr;
	} else {
		return unmappedWrite.data();
	}
}

template<typename Archive>
void MSXToshibaTcx200x::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	ar.serialize("sram", sram,
	             "controlReg", controlReg);
}
INSTANTIATE_SERIALIZE_METHODS(MSXToshibaTcx200x);
REGISTER_MSXDEVICE(MSXToshibaTcx200x, "ToshibaTcx200x");

} // namespace openmsx
