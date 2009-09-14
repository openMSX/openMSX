// $Id$

#include "DummyDevice.hh"
#include "unreachable.hh"

namespace openmsx {

DummyDevice::DummyDevice(MSXMotherBoard& motherBoard, const XMLElement& config)
	: MSXDevice(motherBoard, config)
{
}

void DummyDevice::reset(EmuTime::param /*time*/)
{
	UNREACHABLE;
}

} // namespace openmsx
