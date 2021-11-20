#include "MusicModuleMIDI.hh"
#include "serialize.hh"

namespace openmsx {

// MSX interface:
// port R/W
// 0    W    Control Register
// 1    W    Transmit Data Register
// 4    R    Status Register
// 5    R    Receive Data Register

// Some existing Music-Module detection routines:
// - fac demo 5: does OUT 0,3 : OUT 0,21 : INP(4) and expects to read 2
// - tetris 2 special edition: does INP(4) and expects to read 0
// - Synthesix: does INP(4), expects 0; OUT 0,3 : OUT 0,21: INP(4) and expects
//   bit 1 to be 1 and bit 2, 3 and 7 to be 0. Then does OUT 5,0xFE : INP(4)
//   and expects bit 1 to be 0.
// I did some _very_basic_ investigation and found the following:
// - after a reset reading from port 4 returns 0
// - after initializing the control register, reading port 4 returns 0 (of
//   course this will change when you start to actually receive/transmit data)
// - writing any value with the lower 2 bits set to 1 returns to the initial
//   state, and reading port 4 again returns 0.
// -  ?INP(4) : OUT0,3 : ?INP(4) : OUT0,21 : ? INP(4) : OUT0,3 :  ?INP(4)
//    outputs: 0, 0, 2, 0

MusicModuleMIDI::MusicModuleMIDI(const DeviceConfig& config)
	: MSXDevice(config)
	, mc6850(MSXDevice::getName(), getMotherBoard(), 500000) // 500 kHz
{
	reset(getCurrentTime());
}

void MusicModuleMIDI::reset(EmuTime::param time)
{
	mc6850.reset(time);
}

byte MusicModuleMIDI::readIO(word port, EmuTime::param /*time*/)
{
	switch (port & 0x1) {
	case 0:
		return mc6850.readStatusReg();
	case 1:
		return mc6850.readDataReg();
	}
	UNREACHABLE;
	return 0xFF;
}

byte MusicModuleMIDI::peekIO(word port, EmuTime::param /*time*/) const
{
	switch (port & 0x1) {
	case 0:
		return mc6850.peekStatusReg();
	case 1:
		return mc6850.peekDataReg();
	}
	UNREACHABLE;
	return 0xFF;
}

void MusicModuleMIDI::writeIO(word port, byte value, EmuTime::param time)
{
	switch (port & 0x01) {
	case 0:
		mc6850.writeControlReg(value, time);
		break;
	case 1:
		mc6850.writeDataReg(value, time);
		break;
	}
}

// versions 1-3: were for type "MC6850", see that class for details
// version 4: first version for "MusicModuleMIDI"
template<typename Archive>
void MusicModuleMIDI::serialize(Archive& ar, unsigned version)
{
	ar.template serializeBase<MSXDevice>(*this);
	if (ar.versionAtLeast(version, 4)) {
		ar.serialize("MC6850", mc6850);
	} else {
		// MC6850 members were serialized as direct children of the
		// <device> tag, without an intermediate <MC6850> tag.
		mc6850.serialize(ar, version);
	}
}
INSTANTIATE_SERIALIZE_METHODS(MusicModuleMIDI);
REGISTER_MSXDEVICE(MusicModuleMIDI, "MusicModuleMIDI");

// For backwards compatiblity, also register this class with the old name (only
// needed for loading from disk). Versions 1-3 use the old name "MC6850",
// version 4 (and above) use the name "MusicModuleMIDI".
static RegisterInitializerHelper<XmlInputArchive, MusicModuleMIDI> registerbwCompat("MC6850");

} // namespace openmsx
