// $Id$

#include "MSXDiskRomPatch.hh"
#include "msxconfig.hh"

MSXDiskRomPatch::MSXDiskRomPatch()
{
	// XXX TODO: move consts towards class
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
	switch (address)
	{
		case 0x4010:
		PHYDIO();
		break;

		case 0x4013:
		DSKCHG();
		break;

		case 0x4016:
		GETDPB();
		break;

		case 0x401C:
		DSKFMT();
		break;

		case 0x401F:
		DRVOFF();
		break;
	}
}

void MSXDiskRomPatch::PHYDIO()
{
	// TODO XXX
}
void MSXDiskRomPatch::DSKCHG()
{
	// TODO XXX
}
void MSXDiskRomPatch::GETDPB()
{
	// TODO XXX
}
void MSXDiskRomPatch::DSKFMT()
{
	// TODO XXX
}
void MSXDiskRomPatch::DRVOFF()
{
	// TODO XXX
}
