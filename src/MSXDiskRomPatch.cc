// $Id$

#include "openmsx.hh"
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

void MSXDiskRomPatch::patch() const
{
	PRT_DEBUG("void MSXDiskRomPatch::patch() const");
	int address;
	
	// TODO: get CPU[PC] of patch instruction
	assert(false);
	
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

void MSXDiskRomPatch::PHYDIO() const
{
	// TODO XXX
}
void MSXDiskRomPatch::DSKCHG() const
{
	// TODO XXX
}
void MSXDiskRomPatch::GETDPB() const
{
	// TODO XXX
}
void MSXDiskRomPatch::DSKFMT() const
{
	// TODO XXX
}
void MSXDiskRomPatch::DRVOFF() const
{
	// TODO XXX
}
