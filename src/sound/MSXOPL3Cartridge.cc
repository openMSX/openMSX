#include "MSXOPL3Cartridge.hh"
#include "serialize.hh"
#include "unreachable.hh"

namespace openmsx {

MSXOPL3Cartridge::MSXOPL3Cartridge(const DeviceConfig& config)
	: MSXDevice(config)
	, ymf262(getName(), config, false)
{
	reset(getCurrentTime());
}

void MSXOPL3Cartridge::reset(EmuTime time)
{
	ymf262.reset(time);

	// TODO check
	opl3latch = 0;
}

byte MSXOPL3Cartridge::readIO(uint16_t port, EmuTime /*time*/)
{
	// FM part  0xC4-0xC7 (in MoonSound)
	switch (port & 0x03) {
		case 0: // read status
		case 2:
			return ymf262.readStatus();
		case 1:
		case 3: // read fm register
			return ymf262.readReg(opl3latch);
		default:
			UNREACHABLE;
	}
}

byte MSXOPL3Cartridge::peekIO(uint16_t port, EmuTime /*time*/) const
{
	switch (port & 0x03) {
		case 0: // read status
		case 2:
			return ymf262.peekStatus();
		case 1:
		case 3: // read fm register
			return ymf262.peekReg(opl3latch);
		default:
			UNREACHABLE;
	}
}

void MSXOPL3Cartridge::writeIO(uint16_t port, byte value, EmuTime time)
{
	switch (port & 0x03) {
		case 0: // select register bank 0
			opl3latch = value;
			break;
		case 2: // select register bank 1
			opl3latch = value | 0x100;
			break;
		case 1:
		case 3: // write fm register
			ymf262.writeReg(opl3latch, value, time);
			break;
		default:
			UNREACHABLE;
	}
}

template<typename Archive>
void MSXOPL3Cartridge::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	ar.serialize("ymf262",    ymf262,
	             "opl3latch", opl3latch);
}
INSTANTIATE_SERIALIZE_METHODS(MSXOPL3Cartridge);
REGISTER_MSXDEVICE(MSXOPL3Cartridge, "OPL3Cartridge");

} // namespace openmsx
