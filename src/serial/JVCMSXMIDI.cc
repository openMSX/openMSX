#include "JVCMSXMIDI.hh"
#include "serialize.hh"

namespace openmsx {

// MSX interface:
// port R/W
// 0    R    DIN sync input. Bit0 stop/start, bit 1: clock pulse (NOT EMULATED)
// 2    W    Control Register
// 3    W    Transmit Data Register
// 2    R    Status Register
// 3    R    Receive Data Register

// See information provided by gflorez here:
// https://www.msx.org/forum/msx-talk/hardware/jvc-msx-midi-interface?page=1#comment-377145

// Some existing JVC MSX-MIDI detection routines:
// - Synthesix (copied from MusicModule): does INP(2), expects 0; OUT 2,3 : OUT
//   2,21: INP(2) and expects bit 1 to be 1 and bit 2, 3 and 7 to be 0. Then does
//   OUT 3,0xFE : INP(2) and expects bit 1 to be 0.

JVCMSXMIDI::JVCMSXMIDI(const DeviceConfig& config)
	: MSXDevice(config)
	, mc6850(MSXDevice::getName(), getMotherBoard(), 2000000) // 2 MHz
{
	reset(getCurrentTime());
}

void JVCMSXMIDI::reset(EmuTime::param time)
{
	mc6850.reset(time);
}

byte JVCMSXMIDI::readIO(word port, EmuTime::param /*time*/)
{
	switch (port & 0x7) {
	case 0:
		return 0x00; // NOT IMPLEMENTED
	case 2:
		return mc6850.readStatusReg();
	case 3:
		return mc6850.readDataReg();
	}
	return 0xFF;
}

byte JVCMSXMIDI::peekIO(word port, EmuTime::param /*time*/) const
{
	switch (port & 0x7) {
	case 0:
		return 0x00; // NOT IMPLEMENTED
	case 2:
		return mc6850.peekStatusReg();
	case 3:
		return mc6850.peekDataReg();
	}
	return 0xFF;
}

void JVCMSXMIDI::writeIO(word port, byte value, EmuTime::param time)
{
	switch (port & 0x7) {
	case 2:
		mc6850.writeControlReg(value, time);
		break;
	case 3:
		mc6850.writeDataReg(value, time);
		break;
	}
}

// version 1: initial version
void JVCMSXMIDI::serialize(Archive auto& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	ar.serialize("MC6850", mc6850);
}
INSTANTIATE_SERIALIZE_METHODS(JVCMSXMIDI);
REGISTER_MSXDEVICE(JVCMSXMIDI, "JVCMSXMIDI");

} // namespace openmsx
