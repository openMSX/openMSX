// $Id$

#include "openmsx.hh"
#include "MSXDiskRomPatch.hh"
#include "msxconfig.hh"
#include "CPU.hh"
#include "MSXCPU.hh"
#include "Z80.hh"
#include "MSXMotherBoard.hh"

struct MSXDiskRomPatch::geometry_info MSXDiskRomPatch::geometry[8] =
	{
		{  720, }, // 0
		{ 1440, }, // 1
		{  640, }, // 2
		{ 1280, }, // 3
		{  360, }, // 4
		{  720, }, // 5
		{  320, }, // 6
		{  640, }, // 7
	};

const int MSXDiskRomPatch::sector_size = 512;

const int MSXDiskRomPatch::A_PHYDIO = 0x4010;
const int MSXDiskRomPatch::A_DSKCHG = 0x4013;
const int MSXDiskRomPatch::A_GETDPB = 0x4016;
const int MSXDiskRomPatch::A_DSKFMT = 0x401C;
const int MSXDiskRomPatch::A_DRVOFF = 0x401F;

MSXDiskRomPatch::File::File(std::string _filename):filename(_filename)
{
	file = new IOFILETYPE(filename.c_str(), IOFILETYPE::out|IOFILETYPE::in);
	file->seekg(0,std::ios::end);
	size=file->tellg();
	file->seekg(0,std::ios::beg);
}

MSXDiskRomPatch::File::~File()
{
	delete file;
}

void MSXDiskRomPatch::File::seek(int location)
{
	// seek from beginning of file
	file->seekg(location, std::ios::beg);
}

void MSXDiskRomPatch::File::read(byte* to, int count)
{
	file->read(to, count);
}

void MSXDiskRomPatch::File::write(const byte* from, int count)
{
	file->write(from, count);
}

bool MSXDiskRomPatch::File::bad()
{
	return file->bad();
}

MSXDiskRomPatch::MSXDiskRomPatch()
{
	addr_list.push_back(A_PHYDIO);
	addr_list.push_back(A_DSKCHG);
	addr_list.push_back(A_GETDPB);
	addr_list.push_back(A_DSKFMT);
	addr_list.push_back(A_DRVOFF);
	
	std::string name("diskpatch_diskA");
	//                0123456789ABCDE
	std::string filename;
	for (int i = 0; i < MSXDiskRomPatch::LastDrive; i++)
	{
		disk[i] = 0;
		try
		{
			MSXConfig::Config *config =
				MSXConfig::instance()->getConfigById(name);
			filename = config->getParameter("filename");
			disk[i] = new MSXDiskRomPatch::File(filename);
		}
		catch (MSXException e)
		{
			PRT_DEBUG("void MSXDiskRomPatch::MSXDiskRomPatch() disk exception for disk " << i << " patch: " << name << " filename: " << filename);
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
	MSXMotherBoard* motherboard = MSXMotherBoard::instance();

	CPU::CPURegs regs(cpu->getCPURegs());
	
	regs.IFF1 = true;
	
	// drive #, 0="A:", 1="B:", ..
	byte drive = regs.AF.B.h;
	// number of sectors to read/write
	byte num_sectors = regs.BC.B.h;
	// logical sector number
	int sector_number = regs.DE.w;
	// transfer address
	int transfer_address = regs.HL.w;
	// media descriptor?
	byte media_descriptor = (regs.BC.B.l - 0xF8);
	// read or write, Carry==write
	bool write = (regs.AF.B.l & Z80::C_FLAG);
	PRT_DEBUG("    drive: " << drive);
	PRT_DEBUG("    num_sectors: " << static_cast<int>(num_sectors));
	PRT_DEBUG("    sector_number: " << static_cast<int>(sector_number));
	PRT_DEBUG("    transfer_address: " << std::hex << transfer_address << std::dec);
	PRT_DEBUG("    media_descriptor: " << std::hex << static_cast<int>(media_descriptor) << std::dec);
	PRT_DEBUG("    write/read: " << std::string(write?"write":"read"));

	if (disk[drive] == 0)
	{
		// no disk file -> "Not Ready"
		regs.AF.w = 0x0201;
		cpu->setCPURegs(regs);
		return;
	}

	if (sector_number+num_sectors > MSXDiskRomPatch::geometry[media_descriptor].sectors)
	{
		// out of bound sector -> "Record not found"
		regs.AF.w = 0x0801;
		cpu->setCPURegs(regs);
		return;
	}

	if (transfer_address + num_sectors*MSXDiskRomPatch::sector_size > 0x10000)
	{
		// read would overflow memory, adapt:
		num_sectors = (0x10000 - transfer_address) / MSXDiskRomPatch::sector_size;
	}

	// turn on RAM in all slots?
	// TODO, currently this just assumes RAM is in PS:3/SS:3
	// this is of course quite wrong
	EmuTime dummy(0);
	int pri_slot = motherboard->readIO(0xA8, dummy);
	int sec_slot = cpu->readMem(0xFFFF)^0xFF;
	motherboard->writeIO(0xA8, 0xFF, dummy);
	cpu->writeMem(0xFFFF,0xFF);
	
	byte buffer[MSXDiskRomPatch::sector_size];
	if (write)
	{
		for (int sector = sector_number; num_sectors--; sector++)
		{
			for (int i = 0 ; i < MSXDiskRomPatch::sector_size; i++)
			{
				buffer[i] = cpu->readMem(transfer_address);
				transfer_address++;
			}
			disk[drive]->seek(sector*MSXDiskRomPatch::sector_size);
			disk[drive]->write(buffer, MSXDiskRomPatch::sector_size);
			if (disk[drive]->bad())
			{
				regs.AF.w = 0x0A01;
				motherboard->writeIO(0xA8, pri_slot, dummy);
				cpu->writeMem(0xFFFF,sec_slot);
				cpu->setCPURegs(regs);
				return;
			}
		}
		regs.BC.B.h--;
	}
	else
	{
		for (int sector = sector_number; num_sectors--; sector++)
		{
			disk[drive]->seek(sector*MSXDiskRomPatch::sector_size);
			disk[drive]->read(buffer, MSXDiskRomPatch::sector_size);
			if (disk[drive]->bad())
			{
				regs.AF.w = 0x0A01;
				motherboard->writeIO(0xA8, pri_slot, dummy);
				cpu->writeMem(0xFFFF,sec_slot);
				cpu->setCPURegs(regs);
				return;
			}
			for (int i = 0 ; i < MSXDiskRomPatch::sector_size; i++)
			{
				cpu->writeMem(transfer_address, buffer[i]);
				transfer_address++;
			}
		}
		regs.BC.B.h--;
	}

	motherboard->writeIO(0xA8, pri_slot, dummy);
	cpu->writeMem(0xFFFF,sec_slot);
	regs.AF.B.l &= (~Z80::C_FLAG);
	cpu->setCPURegs(regs);
}
void MSXDiskRomPatch::DSKCHG() const
{
	PRT_DEBUG("void MSXDiskRomPatch::DRVOFF() const");

	CPU* cpu = MSXCPU::instance()->getActiveCPU();
	CPU::CPURegs regs(cpu->getCPURegs());

	regs.IFF1 = true;

	// drive #, 0="A:", 1="B:", ..
	byte drive = regs.AF.B.h;

	PRT_DEBUG("    drive: " << drive);

	if (disk[drive] == 0)
	{
		// no disk file -> "Not Ready"
		regs.AF.w = 0x0201;
		cpu->setCPURegs(regs);
		return;
	}

	regs.BC.B.h  = 0;
	regs.AF.B.l &= (~Z80::C_FLAG);
	cpu->setCPURegs(regs);
	GETDPB();
}
void MSXDiskRomPatch::GETDPB() const
{
	PRT_DEBUG("void MSXDiskRomPatch::GETDPB() const");
	
	CPU* cpu = MSXCPU::instance()->getActiveCPU();
	CPU::CPURegs regs(cpu->getCPURegs());

	regs.IFF1 = true;

	// drive #, 0="A:", 1="B:", ..
	byte drive = regs.AF.B.h;

	// media descriptor?
	byte media_descriptor1 = regs.BC.B.h;
	byte media_descriptor2 = regs.BC.B.l;

	int DPB_base_address = regs.HL.w;

	PRT_DEBUG("    drive: " << drive);
	PRT_DEBUG("    media_descriptor1: " << std::hex << static_cast<int>(media_descriptor1) << std::dec);
	PRT_DEBUG("    media_descriptor2: " << std::hex << static_cast<int>(media_descriptor2) << std::dec);
	PRT_DEBUG("    DPB_base_address: " << std::hex << DPB_base_address << std::dec);

	if (disk[drive] == 0)
	{
		// no disk file -> "Not Ready"
		regs.AF.w = 0x0201;
		cpu->setCPURegs(regs);
		return;
	}

	byte buffer[MSXDiskRomPatch::sector_size];
	disk[drive]->seek(0);
	disk[drive]->read(buffer, MSXDiskRomPatch::sector_size);

	int bytes_per_sector = (int)(buffer[0x0C]*0x100 + buffer[0x0B]);
	int sectors_per_disk = (int)(buffer[0x14]*0x100 + buffer[0x13]);
	int sectors_per_FAT  = (int)(buffer[0x17]*0x100 + buffer[0x16]);
	int reserved_sectors = (int)(buffer[0x0F]*0x100 + buffer[0x0E]);

	cpu->writeMem(DPB_base_address + 0x01 , buffer[0x15]); // media type
	cpu->writeMem(DPB_base_address + 0x02 , buffer[0x0B]); // sector size
	cpu->writeMem(DPB_base_address + 0x03 , buffer[0x0C]);
	// TODO rewrite next 2 lines to be readable: XXX
	int bytes_per_sector_shift = (bytes_per_sector >> 5)-1;
	int i;
	for (i = 0; bytes_per_sector_shift & (1 << i); i++);
	cpu->writeMem(DPB_base_address + 0x04 , bytes_per_sector_shift); // directory mask/shift
	cpu->writeMem(DPB_base_address + 0x05 , i);
	int cluster_mask_shift = buffer[0x0D]-1;
	for (i = 0; cluster_mask_shift & (1 << i); i++);
	cpu->writeMem(DPB_base_address + 0x06 , cluster_mask_shift); // cluster mask/shift
	cpu->writeMem(DPB_base_address + 0x07 , i+1);
	cpu->writeMem(DPB_base_address + 0x08 , buffer[0x0E]); // sector # of
	cpu->writeMem(DPB_base_address + 0x09 , buffer[0x0F]); // first FAT
	cpu->writeMem(DPB_base_address + 0x0A , buffer[0x10]); // # of FATS
	cpu->writeMem(DPB_base_address + 0x0B , buffer[0x11]); // # of directory entries
	int sect_num_of_data =
		reserved_sectors + buffer[0x10]*sectors_per_FAT
		+ 32*buffer[0x11]/bytes_per_sector;
	cpu->writeMem(DPB_base_address + 0x0C, sect_num_of_data & 0xFF); // sector number of data
	cpu->writeMem(DPB_base_address + 0x0D, (sect_num_of_data>>8) & 0xFF);
	cpu->writeMem(DPB_base_address + 0x0E, buffer[0x16]); // sectors per fat
	int sect_num_of_dir =
		reserved_sectors + buffer[0x10]*sectors_per_FAT;
	cpu->writeMem(DPB_base_address + 0x0F, sect_num_of_dir & 0xFF); // sector # of dir
	cpu->writeMem(DPB_base_address + 0x10, (sect_num_of_dir>>8) & 0xFF);

	regs.AF.B.l &= (~Z80::C_FLAG);
	cpu->setCPURegs(regs);
}
void MSXDiskRomPatch::DSKFMT() const
{
	assert(false);
	// TODO XXX
}
void MSXDiskRomPatch::DRVOFF() const
{
	PRT_DEBUG("void MSXDiskRomPatch::DRVOFF() const [does nothing]");

	// do nothing
}
