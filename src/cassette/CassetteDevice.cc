// $Id$

#include "CassetteDevice.hh"


const std::string &CassetteDevice::getClass()
{
	static const std::string className("Cassette Port");
	return className;
}
