
#include "MSXMapperIOTurboR.hh"
#include <assert.h>

MSXMapperIOTurboR::MSXMapperIOTurboR()
{
}

MSXMapperIOTurboR::~MSXMapperIOTurboR()
{
}

// 3 most significant bits read always "1"
byte MSXMapperIOTurboR::readIO(byte port, Emutime &time)
{
	assert (0xfc <= port);
	int pageNum = getPageNum(port-0xfc);
	return pageNum|0xe0;
}

void  MSXMapperIOTurboR::registerMapper(int blocks)
{
	// do nothing
}
