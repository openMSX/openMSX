// $Id$

#include "CassetteDevice.hh"


const std::string &CassetteDevice::getClass() const
{
	static const std::string className("Cassette Port");
	return className;
}
