// $Id$

#include "CassetteDevice.hh"

using std::string;

namespace openmsx {

const string& CassetteDevice::getClass() const
{
	static const string className("Cassette Port");
	return className;
}

} // namespace openmsx
