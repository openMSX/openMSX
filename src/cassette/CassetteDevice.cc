// $Id$

#include "CassetteDevice.hh"


namespace openmsx {

const string &CassetteDevice::getClass() const
{
	static const string className("Cassette Port");
	return className;
}

} // namespace openmsx
