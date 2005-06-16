// $Id$

#include "DummyDevice.hh"
#include <cassert>

namespace openmsx {

DummyDevice::DummyDevice(MSXMotherBoard& motherBoard, const XMLElement& config,
                         const EmuTime& time)
	: MSXDevice(motherBoard, config, time)
{
}

DummyDevice::~DummyDevice()
{
}

void DummyDevice::reset(const EmuTime& /*time*/)
{
	// Block usage
	assert(false);
}

} // namespace openmsx
