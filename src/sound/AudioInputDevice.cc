// $Id$

#include "AudioInputDevice.hh"


namespace openmsx {

const string &AudioInputDevice::getClass() const
{
	static const string className("Audio Input Port");
	return className;
}

} // namespace openmsx
