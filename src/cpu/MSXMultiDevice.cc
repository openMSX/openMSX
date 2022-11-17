#include "MSXMultiDevice.hh"
#include "DeviceConfig.hh"
#include "XMLElement.hh"
#include "unreachable.hh"

namespace openmsx {

static DeviceConfig getMultiConfig(const HardwareConfig& hwConf)
{
	static const XMLElement* xml = [] {
		auto& doc = XMLDocument::getStaticDocument();
		return doc.allocateElement("Multi");
	}();
	return {hwConf, *xml};
}

MSXMultiDevice::MSXMultiDevice(const HardwareConfig& hwConf)
	: MSXDevice(getMultiConfig(hwConf), "Multi")
{
}

void MSXMultiDevice::reset(EmuTime::param /*time*/)
{
	UNREACHABLE;
}

void MSXMultiDevice::powerUp(EmuTime::param /*time*/)
{
	UNREACHABLE;
}

void MSXMultiDevice::powerDown(EmuTime::param /*time*/)
{
	UNREACHABLE;
}

} // namespace openmsx
