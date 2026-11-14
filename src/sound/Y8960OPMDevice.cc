#include "Y8960OPMDevice.hh"

#include "serialize.hh"

namespace openmsx {

Y8960OPMDevice::Y8960OPMDevice(const DeviceConfig& config)
	: MSXDevice(config)
	, ym2151(getName(), "Y8960 OPM", config, getCurrentTime(), YM2151::Variant::YM2151)
	, registerLatch(0)
	, ioEnabled(false)
{
	reset(getCurrentTime());
}

void Y8960OPMDevice::reset(EmuTime time)
{
	ym2151.reset(time);
	registerLatch = 0;
	ioEnabled = false;
}

byte Y8960OPMDevice::readIO(uint16_t port, EmuTime time)
{
	return peekIO(port, time);
}

byte Y8960OPMDevice::peekIO(uint16_t port, EmuTime /*time*/) const
{
	if (!ioEnabled) return 0xFF;
	// The cartridge answers the status on the data port only; a read of the
	// address port leaves the bus alone.
	// The busy bit (b7) stays 0: a write here takes effect at once, whereas
	// the cartridge queues writes and shows the queue as busy. Software that
	// waits for busy to clear works either way.
	return (port & 1) ? ym2151.readStatus() : 0xFF;
}

void Y8960OPMDevice::writeIO(uint16_t port, byte value, EmuTime time)
{
	if (!ioEnabled) return;
	writePort((port & 1) != 0, value, time);
}

void Y8960OPMDevice::writePort(bool port, byte value, EmuTime time)
{
	if (port) {
		ym2151.writeReg(registerLatch, value, time);
	} else {
		registerLatch = value;
	}
}

template<typename Archive>
void Y8960OPMDevice::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	ar.serialize("ym2151",        ym2151,
	             "registerLatch", registerLatch,
	             "ioEnabled",     ioEnabled);
}
INSTANTIATE_SERIALIZE_METHODS(Y8960OPMDevice);
REGISTER_MSXDEVICE(Y8960OPMDevice, "Y8960-OPM");

} // namespace openmsx
