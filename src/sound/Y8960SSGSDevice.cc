#include "Y8960SSGSDevice.hh"

#include "serialize.hh"

namespace openmsx {

Y8960SSGSDevice::Y8960SSGSDevice(const DeviceConfig& config)
	: MSXDevice(config)
	, registerLatch(0)
	, ioEnabled(false)
	, ssgs(getName(), config, getCurrentTime())
{
	reset(getCurrentTime());
}

void Y8960SSGSDevice::reset(EmuTime time)
{
	ssgs.reset(time);
	registerLatch = 0;
	ioEnabled = false;
}

void Y8960SSGSDevice::writeIO(uint16_t port, byte value, EmuTime time)
{
	if (!ioEnabled) return;
	// A2h is the read port and A3h is spare; writes to them do nothing.
	switch (port & 3) {
	case 0: writePort(false, value, time); break;
	case 1: writePort(true,  value, time); break;
	default: break;
	}
}

void Y8960SSGSDevice::writePort(bool port, byte value, EmuTime time)
{
	if (port) {
		ssgs.writeRegister(registerLatch, value, time);
	} else {
		registerLatch = value & 0x3F;
	}
}

template<typename Archive>
void Y8960SSGSDevice::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	// The tag is part of the savestate format, so it stays "ssg" even though
	// the member no longer is.
	ar.serialize("ssg",           ssgs,
	             "registerLatch", registerLatch,
	             "ioEnabled",     ioEnabled);
}
INSTANTIATE_SERIALIZE_METHODS(Y8960SSGSDevice);
REGISTER_MSXDEVICE(Y8960SSGSDevice, "Y8960-SSGS");

} // namespace openmsx
