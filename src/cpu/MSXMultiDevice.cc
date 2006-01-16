// $Id$

#include "MSXMultiDevice.hh"
#include "EmuTime.hh"
#include "FileContext.hh"
#include "XMLElement.hh"
#include <cassert>

namespace openmsx {

static const XMLElement& getMultiConfig()
{
	static XMLElement deviceElem("Multi");
	static bool init = false;
	if (!init) {
		init = true;
		deviceElem.setFileContext(
			std::auto_ptr<FileContext>(new SystemFileContext()));
	}
	return deviceElem;
}

MSXMultiDevice::MSXMultiDevice(MSXMotherBoard& motherboard)
	: MSXDevice(motherboard, getMultiConfig(), EmuTime::zero, "Multi")
{
}

void MSXMultiDevice::reset(const EmuTime& /*time*/)
{
	assert(false);
}

void MSXMultiDevice::powerUp(const EmuTime& /*time*/)
{
	assert(false);
}

void MSXMultiDevice::powerDown(const EmuTime& /*time*/)
{
	assert(false);
}

} // namespace openmsx
