// $Id$

#include "openmsx.hh"
#include "MSXDiskRomPatch.hh"
#include "msxconfig.hh"
#include "CPU.hh"
#include "MSXCPU.hh"

MSXDiskRomPatch::MSXDiskRomPatch()
{
	// XXX TODO: move consts towards class
	addr_list.push_back(A_PHYDIO);
	addr_list.push_back(A_DSKCHG);
	addr_list.push_back(A_GETDPB);
	addr_list.push_back(A_DSKFMT);
	addr_list.push_back(A_DRVOFF);
}

MSXDiskRomPatch::~MSXDiskRomPatch()
{
}

void MSXDiskRomPatch::patch() const
{
	PRT_DEBUG("void MSXDiskRomPatch::patch() const");
	
	CPU &cpu = MSXCPU::instance()->getActiveCPU();
	CPU::CPURegs regs(cpu.getCPURegs());

	int address = regs.PC.w;
	PRT_DEBUG("regs.PC==" << address);
	return;
	
	// TODO: get CPU[PC] of patch instruction
	assert(false);
	
	switch (address)
	{
		case A_PHYDIO:
		PHYDIO();
		break;

		case A_DSKCHG:
		DSKCHG();
		break;

		case A_GETDPB:
		GETDPB();
		break;

		case A_DSKFMT:
		DSKFMT();
		break;

		case A_DRVOFF:
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
