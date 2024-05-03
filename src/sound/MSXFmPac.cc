#include "MSXFmPac.hh"
#include "CacheLine.hh"
#include "serialize.hh"

namespace openmsx {

static constexpr const char* const PAC_Header = "PAC2 BACKUP DATA";

MSXFmPac::MSXFmPac(const DeviceConfig& config)
	: MSXMusicBase(config)
	, sram(getName() + " SRAM", 0x1FFE, config, PAC_Header)
	, romBlockDebug(*this, std::span{&bank, 1}, 0x4000, 0x4000, 14)
{
	reset(getCurrentTime());
}

void MSXFmPac::reset(EmuTime::param time)
{
	MSXMusicBase::reset(time);
	enable = 0;
	sramEnabled = false;
	bank = 0;
	r1ffe = r1fff = 0; // actual value doesn't matter as long
	                   // as it's not the magic combination
}

void MSXFmPac::writeIO(word port, byte value, EmuTime::param time)
{
	if (enable & 1) {
		MSXMusicBase::writeIO(port, value, time);
	}
}

byte MSXFmPac::readMem(word address, EmuTime::param /*time*/)
{
	address &= 0x3FFF;
	switch (address) {
	case 0x3FF6:
		return enable;
	case 0x3FF7:
		return bank;
	default:
		if (sramEnabled) {
			if (address < 0x1FFE) {
				return sram[address];
			} else if (address == 0x1FFE) {
				return r1ffe; // always 0x4D
			} else if (address == 0x1FFF) {
				return r1fff; // always 0x69
			} else {
				return 0xFF;
			}
		} else {
			return rom[bank * 0x4000 + address];
		}
	}
}

const byte* MSXFmPac::getReadCacheLine(word address) const
{
	address &= 0x3FFF;
	if (address == (0x3FF6 & CacheLine::HIGH)) {
		return nullptr;
	}
	if (sramEnabled) {
		if (address < (0x1FFE & CacheLine::HIGH)) {
			return &sram[address];
		} else if (address == (0x1FFE & CacheLine::HIGH)) {
			return nullptr;
		} else {
			return unmappedRead.data();
		}
	} else {
		return &rom[bank * 0x4000 + address];
	}
}

void MSXFmPac::writeMem(word address, byte value, EmuTime::param time)
{
	// 'enable' has no effect for memory mapped access
	//   (thanks to BiFiMSX for investigating this)
	address &= 0x3FFF;
	switch (address) {
	case 0x1FFE:
		if (!(enable & 0x10)) {
			r1ffe = value;
			checkSramEnable();
		}
		break;
	case 0x1FFF:
		if (!(enable & 0x10)) {
			r1fff = value;
			checkSramEnable();
		}
		break;
	case 0x3FF4: // address
	case 0x3FF5: // data
		writePort(address & 1, value, time);
		break;
	case 0x3FF6:
		enable = value & 0x11;
		if (enable & 0x10) {
			r1ffe = r1fff = 0; // actual value not important
			checkSramEnable();
		}
		break;
	case 0x3FF7: {
		if (byte newBank = value & 0x03; bank != newBank) {
			bank = newBank;
			invalidateDeviceRCache();
		}
		break;
	}
	default:
		if (sramEnabled && (address < 0x1FFE)) {
			sram.write(address, value);
		}
	}
}

byte* MSXFmPac::getWriteCacheLine(word address)
{
	address &= 0x3FFF;
	if (address == (0x1FFE & CacheLine::HIGH)) {
		return nullptr;
	}
	if (address == (0x3FF4 & CacheLine::HIGH)) {
		return nullptr;
	}
	if (sramEnabled && (address < 0x1FFE)) {
		return nullptr;
	} else {
		return unmappedWrite.data();
	}
}

void MSXFmPac::checkSramEnable()
{
	bool newEnabled = (r1ffe == 0x4D) && (r1fff == 0x69);
	if (sramEnabled != newEnabled) {
		sramEnabled = newEnabled;
		invalidateDeviceRWCache();
	}
}


template<typename Archive>
void MSXFmPac::serialize(Archive& ar, unsigned version)
{
	ar.template serializeInlinedBase<MSXMusicBase>(*this, version);
	ar.serialize("sram",   sram,
	             "enable", enable,
	             "bank",   bank,
	             "r1ffe",  r1ffe,
	             "r1fff",  r1fff);
	if constexpr (Archive::IS_LOADER) {
		// sramEnabled can be calculated
		checkSramEnable();
	}
}
INSTANTIATE_SERIALIZE_METHODS(MSXFmPac);
REGISTER_MSXDEVICE(MSXFmPac, "FM-PAC");

} // namespace openmsx
