// $Id$

#include "DummyDevice.hh"
#include <cassert>

namespace openmsx {

DummyDevice::DummyDevice(MSXMotherBoard& motherBoard, const XMLElement& config)
	: MSXDevice(motherBoard, config)
{
}

void DummyDevice::reset(const EmuTime& /*time*/)
{
	// Block usage
	assert(false);
}

} // namespace openmsx
