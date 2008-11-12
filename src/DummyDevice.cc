// $Id$

#include "DummyDevice.hh"
#include <cassert>

namespace openmsx {

DummyDevice::DummyDevice(MSXMotherBoard& motherBoard, const XMLElement& config)
	: MSXDevice(motherBoard, config)
{
}

void DummyDevice::reset(EmuTime::param /*time*/)
{
	// Block usage
	assert(false);
}

} // namespace openmsx
