#include "Y8960OPLLDevice.hh"

#include "serialize.hh"

namespace openmsx {

Y8960OPLLDevice::Y8960OPLLDevice(const DeviceConfig& config)
	: MSXDevice(config)
	, opll(getName(), config)
	, ioEnabled(false)
{
	reset(getCurrentTime());
}

void Y8960OPLLDevice::reset(EmuTime time)
{
	opll.reset(time);
	ioEnabled = false;
}

void Y8960OPLLDevice::writeIO(uint16_t port, byte value, EmuTime time)
{
	if (!ioEnabled) return;
	writePort(port & 1, value, time);
}

void Y8960OPLLDevice::writePort(bool port, byte value, EmuTime time)
{
	opll.writePort(port, value, time);
}

template<typename Archive>
void Y8960OPLLDevice::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	ar.serialize("opll",      opll,
	             "ioEnabled", ioEnabled);
}
INSTANTIATE_SERIALIZE_METHODS(Y8960OPLLDevice);
REGISTER_MSXDEVICE(Y8960OPLLDevice, "Y8960-OPLL");

} // namespace openmsx
