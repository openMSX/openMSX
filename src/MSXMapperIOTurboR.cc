// $Id$

#include "MSXMapperIOTurboR.hh"

// 3 most significant bits read always "1"
byte MSXMapperIOTurboR::convert(byte value)
{
	return value|0xe0;
}

void  MSXMapperIOTurboR::registerMapper(int blocks)
{
	// do nothing
}
