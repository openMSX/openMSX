// $Id$

#include "MSXMultiDevice.hh"
#include "XMLElement.hh"
#include <cassert>

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
	assert(false);
}

void MSXMultiDevice::powerUp(EmuTime::param /*time*/)
{
	assert(false);
}

void MSXMultiDevice::powerDown(EmuTime::param /*time*/)
{
	assert(false);
}

} // namespace openmsx
