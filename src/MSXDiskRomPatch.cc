// $Id$

#include "MSXDiskRomPatch.hh"
#include "msxconfig.hh"

MSXDiskRomPatch::MSXDiskRomPatch()
{
	addr_list.push_back(0x4010);
	addr_list.push_back(0x4013);
	addr_list.push_back(0x4016);
	addr_list.push_back(0x401C);
	addr_list.push_back(0x401F);
}

MSXDiskRomPatch::~MSXDiskRomPatch()
{
}

void MSXDiskRomPatch::patch(int address)
{
	// TODO
}
