#include "MSXRTC.hh"
#include "serialize.hh"
#include "unreachable.hh"

namespace openmsx {

MSXRTC::MSXRTC(const DeviceConfig& config)
	: MSXDevice(config)
	, sram(getName() + " SRAM", 4 * 13, config)
	, rp5c01(getCommandController(), sram, getCurrentTime(), getName())
{
	reset(getCurrentTime());
}

void MSXRTC::reset(EmuTime::param time)
{
	// TODO verify on real hardware .. how?
	//  - registerLatch set to zero or some other value?
	//  - only on power-up or also on reset?
	registerLatch = 0;
	rp5c01.reset(time);
}

uint8_t MSXRTC::readIO(uint16_t port, EmuTime::param time)
{
	return port & 0x01 ? rp5c01.readPort(registerLatch, time) | 0xF0 : 0xFF;
}

uint8_t MSXRTC::peekIO(uint16_t port, EmuTime::param /*time*/) const
{
	return port & 0x01 ? rp5c01.peekPort(registerLatch) | 0xF0 : 0xFF;
}

void MSXRTC::writeIO(uint16_t port, uint8_t value, EmuTime::param time)
{
	switch (port & 0x01) {
	case 0:
		registerLatch = value & 0x0F;
		break;
	case 1:
		rp5c01.writePort(registerLatch, value & 0x0F, time);
		break;
	default:
		UNREACHABLE;
	}
}

template<typename Archive>
void MSXRTC::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	ar.serialize("sram",          sram,
	             "rp5c01",        rp5c01,
	             "registerLatch", registerLatch);
}
INSTANTIATE_SERIALIZE_METHODS(MSXRTC);
REGISTER_MSXDEVICE(MSXRTC, "RTC");

} // namespace openmsx
