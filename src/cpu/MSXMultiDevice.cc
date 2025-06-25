#include "MSXMultiDevice.hh"
#include "DeviceConfig.hh"
#include "XMLElement.hh"
#include "unreachable.hh"

namespace openmsx {

static DeviceConfig getMultiConfig(HardwareConfig& hwConf)
{
	static XMLElement* xml = [] {
		auto& doc = XMLDocument::getStaticDocument();
		return doc.allocateElement("Multi");
	}();
	return {hwConf, *xml};
}

MSXMultiDevice::MSXMultiDevice(HardwareConfig& hwConf)
	: MSXDevice(getMultiConfig(hwConf), "Multi")
{
}

void MSXMultiDevice::reset(EmuTime /*time*/)
{
	UNREACHABLE;
}

void MSXMultiDevice::powerUp(EmuTime /*time*/)
{
	UNREACHABLE;
}

void MSXMultiDevice::powerDown(EmuTime /*time*/)
{
	UNREACHABLE;
}

} // namespace openmsx
