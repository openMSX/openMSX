// $Id$

#include "MSXMultiDevice.hh"
#include "XMLElement.hh"
#include "unreachable.hh"

namespace openmsx {

static const XMLElement& getMultiConfig()
{
	static XMLElement deviceElem("Multi");
	return deviceElem;
}

MSXMultiDevice::MSXMultiDevice(MSXMotherBoard& motherboard)
	: MSXDevice(motherboard, getMultiConfig(), "Multi")
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
