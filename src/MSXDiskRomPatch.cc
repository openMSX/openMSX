// $Id$

#include "openmsx.hh"
#include "MSXDiskRomPatch.hh"
#include "msxconfig.hh"
#include "CPU.hh"
#include "MSXCPU.hh"
#include "Z80.hh"

MSXDiskRomPatch::File::File(std::string _filename):filename(_filename)
{
	file = new IOFILETYPE(filename.c_str(), IOFILETYPE::out|IOFILETYPE::in);
	file->seekg(0,ios::end);
	size=file->tellg();
	file->seekg(0,ios::beg);
}

MSXDiskRomPatch::File::~File()
{
	delete file;
}

void MSXDiskRomPatch::File::seek(int location)
{
	// seek from beginning of file
	file->seekg(location, ios::beg);
}

void MSXDiskRomPatch::File::read(byte* to, int count)
{
	file->read(to, count);
}

void MSXDiskRomPatch::File::write(const byte* from, int count)
{
	file->write(from, count);
}


MSXDiskRomPatch::MSXDiskRomPatch()
{
	addr_list.push_back(A_PHYDIO);
	addr_list.push_back(A_DSKCHG);
	addr_list.push_back(A_GETDPB);
	addr_list.push_back(A_DSKFMT);
	addr_list.push_back(A_DRVOFF);
	
	std::string name("diskpatch_diskA");
	//          0123456789ABCDE
	for (int i = 0; i < MSXDiskRomPatch::LastDrive; i++)
	{
		disk[i] = 0;
		try
		{
			MSXConfig::Config *config =
				MSXConfig::instance()->getConfigById(name);
			std::string filename = config->getParameter("filename");
			disk[i] = new MSXDiskRomPatch::File(filename);
		}
		catch (MSXException e)
		{
			PRT_DEBUG("void MSXDiskRomPatch::MSXDiskRomPatch() disk exception for disk " << i);
			delete disk[0];
			disk[i] = 0;
		}
		// next drive letter
		name[0xE]++;
	}
}

MSXDiskRomPatch::~MSXDiskRomPatch()
{
	for (int i = 0; i < MSXDiskRomPatch::LastDrive; i++)
	{
		if (disk[i] != 0) delete disk[i];
	}
}

void MSXDiskRomPatch::patch() const
{
	PRT_DEBUG("void MSXDiskRomPatch::patch() const");
	
	CPU* cpu = MSXCPU::instance()->getActiveCPU();
	CPU::CPURegs regs(cpu->getCPURegs());

	int address = regs.PC.w-2;
	
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
	PRT_DEBUG("void MSXDiskRomPatch::PHYDIO() const");
	
	CPU* cpu = MSXCPU::instance()->getActiveCPU();
	CPU::CPURegs regs(cpu->getCPURegs());
	
	// drive #, 0="A:", 1="B:", ..
	byte drive = regs.AF.B.h;
	// number of sectors to read/write
	byte num_sectors = regs.BC.B.h;
	// logical sector number
	int sector_number = regs.DE.w;
	// transfer address
	int transfer_address = regs.HL.w;
	// media descriptor?
	byte media_descriptor = regs.BC.B.l;
	// read or write, Carry==write
	bool write = (regs.AF.B.l & Z80::C_FLAG);
	PRT_DEBUG("    drive: " << drive);
	PRT_DEBUG("    num_sectors: " << static_cast<int>(num_sectors));
	PRT_DEBUG("    sector_number: " << static_cast<int>(sector_number));
	PRT_DEBUG("    transfer_address: " << std::hex << transfer_address << std::dec);
	PRT_DEBUG("    media_descriptor: " << std::hex << static_cast<int>(media_descriptor) << std::dec);
	PRT_DEBUG("    write/read: " << std::string(write?"write":"read"));

	// WIP
	assert(false);
}
void MSXDiskRomPatch::DSKCHG() const
{
	assert(false);
	// TODO XXX
}
void MSXDiskRomPatch::GETDPB() const
{
	assert(false);
	// TODO XXX
}
void MSXDiskRomPatch::DSKFMT() const
{
	assert(false);
	// TODO XXX
}
void MSXDiskRomPatch::DRVOFF() const
{
	PRT_DEBUG("void MSXDiskRomPatch::DRVOFF() const");

	// do nothing
}
