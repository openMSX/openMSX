// $Id$

#include "CassetteDevice.hh"


const string &CassetteDevice::getClass() const
{
	static const string className("Cassette Port");
	return className;
}
