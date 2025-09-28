#include "GoudaSCSI.hh"

#include "serialize.hh"

#include "unreachable.hh"

namespace openmsx {

GoudaSCSI::GoudaSCSI(DeviceConfig& config)
	: MSXDevice(config)
	, rom(getName() + " ROM", "rom", config)
	, wd33c93(config)
{
	reset(EmuTime::dummy());
}

void GoudaSCSI::reset(EmuTime /*time*/)
{
	wd33c93.reset(true);
}

byte GoudaSCSI::readIO(uint16_t port, EmuTime /*time*/)
{
	switch (port & 0x03) {
	case 0:
		return wd33c93.readAuxStatus();
	case 1:
		return wd33c93.readCtrl();
	case 2:
		return 0xb0; // bit 4: 1 = Halt on SCSI parity error
	default:
		UNREACHABLE;
	}
}

byte GoudaSCSI::peekIO(uint16_t port, EmuTime /*time*/) const
{
	switch (port & 0x03) {
	case 0:
		return wd33c93.peekAuxStatus();
	case 1:
		return wd33c93.peekCtrl();
	case 2:
		return 0xb0; // bit 4: 1 = Halt on SCSI parity error
	default:
		UNREACHABLE;
	}
}

void GoudaSCSI::writeIO(uint16_t port, byte value, EmuTime time)
{
	switch (port & 0x03) {
	case 0:
		wd33c93.writeAdr(value);
		break;
	case 1:
		wd33c93.writeCtrl(value);
		break;
	case 2:
		reset(time);
		break;
	default:
		UNREACHABLE;
	}
}

byte GoudaSCSI::readMem(uint16_t address, EmuTime /*time*/)
{
	return *GoudaSCSI::getReadCacheLine(address);
}

const byte* GoudaSCSI::getReadCacheLine(uint16_t start) const
{
	return &rom[start & (rom.size() - 1)];
}


template<typename Archive>
void GoudaSCSI::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	ar.serialize("WD33C93", wd33c93);
}
INSTANTIATE_SERIALIZE_METHODS(GoudaSCSI);
REGISTER_MSXDEVICE(GoudaSCSI, "GoudaSCSI");

} // namespace openmsx
