// $Id$

#include "MSXRomPatchInterface.hh"

const std::list<int> &MSXRomPatchInterface::addresses()
{
	return addr_list;
}

MSXRomPatchInterface::~MSXRomPatchInterface()
{
}
