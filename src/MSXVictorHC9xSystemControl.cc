#include "MSXVictorHC9xSystemControl.hh"
#include "serialize.hh"
#include <cassert>

// This implementation is documented in the HC-95 service manual:
//
// System control:
// 7FFD  I/O      bit 0    NMI CONTROL         "0" ENABLE
//       I/O      bit 1    INT CONTROL         "1" ENABLE
//       I/O      bit 2    WAIT CONTROL        "0" ENABLE
//       I/O      bit 3    JVC MODE ACK FLAG   "1" JVC ACK
//       I/O      bit 4    FDC DREQ CONTROL    "1" ENABLE
//       I/O      bit 5    RS-232C FLAG        "1" ENABLE
//       I        bit 6    TURBO MODE FLAG     "1" TURBO
//       I        bit 7    JVC MODE FLAG       "0" JVC MODE

// turbo mode is determined by a physical switch on the machine (hence readonly)

namespace openmsx {

MSXVictorHC9xSystemControl::MSXVictorHC9xSystemControl(const DeviceConfig& config)
	: MSXDevice(config)
{
}

byte MSXVictorHC9xSystemControl::readMem(word address, EmuTime::param time)
{
	return peekMem(address, time);
}

byte MSXVictorHC9xSystemControl::peekMem(word address, EmuTime::param /*time*/) const
{
	(void)address; // avoid warning for non-assert compiles
	assert (address == 0x7FFD);
	return systemControlRegister;
}

void MSXVictorHC9xSystemControl::writeMem(word address, byte value, EmuTime::param /*time*/) {
	(void)address; // avoid warning for non-assert compiles
	assert (address == 0x7FFD);
	systemControlRegister = (value & 0x3F) | 0x80;
}

bool MSXVictorHC9xSystemControl::allowUnaligned() const
{
	// OK, because this device doesn't call any 'fillDeviceXXXCache()'functions.
	return true;
}

template<typename Archive>
void MSXVictorHC9xSystemControl::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	ar.serialize("systemControlRegister", systemControlRegister);
}

INSTANTIATE_SERIALIZE_METHODS(MSXVictorHC9xSystemControl);
REGISTER_MSXDEVICE(MSXVictorHC9xSystemControl, "VictorHC9xSystemControl");

} // namespace openmsx
