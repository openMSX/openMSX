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
	
	// stolen from Z80::ei()
	regs.IFF1 = false;		// no ints after this instruction
	regs.nextIFF1 = true;	// but allow them after next instruction
	regs.IFF2 = true;
	
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
#ifdef DEBUG
	std::string driveletter("A");
	driveletter[0] += drive;
#endif
	PRT_DEBUG("    drive: " << driveletter << ":");

	if (drive >= MSXDiskRomPatch::LastDrive)
	{
		PRT_DEBUG("    Illegal Drive letter " << driveletter << ":");
#ifdef DEBUG
		assert(false);
#endif
		// illegal drive letter -> "Not Ready"
		regs.AF.w = 0x0201;
		cpu->setCPURegs(regs);
		return;
	}

	PRT_DEBUG("    num_sectors: " << static_cast<int>(num_sectors));
	PRT_DEBUG("    sector_number: " << static_cast<int>(sector_number));
	PRT_DEBUG("    transfer_address: 0x" << std::hex << transfer_address << std::dec);
	PRT_DEBUG("    media_descriptor: 0x" << std::hex << static_cast<int>(media_descriptor) << std::dec);
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
	PRT_DEBUG("Primary: "
		<< "s3:" << ((pri_slot & 0xC0)>>6) << " "
		<< "s2:" << ((pri_slot & 0x30)>>4) << " "
		<< "s1:" << ((pri_slot & 0x0C)>>2) << " "
		<< "s0:" << ((pri_slot & 0x03)>>0));
	PRT_DEBUG("Secundary: "
		<< "s3:" << ((sec_slot & 0xC0)>>6) << " "
		<< "s2:" << ((sec_slot & 0x30)>>4) << " "
		<< "s1:" << ((sec_slot & 0x0C)>>2) << " "
		<< "s0:" << ((sec_slot & 0x03)>>0));

	int pri_slot_target = ((pri_slot & 0xC0)>>6);
	pri_slot_target += pri_slot_target*4 + pri_slot_target*16 + pri_slot_target*64;
	int sec_slot_target = ((sec_slot & 0xC0)>>6);
	sec_slot_target += sec_slot_target*4 + sec_slot_target*16 + sec_slot_target*64;
	PRT_DEBUG("Switching slots toward: pri:0x" << std::hex
		<< pri_slot_target
		<< " sec:0x" << sec_slot_target << std::dec);

	motherboard->writeIO(0xA8, pri_slot_target, dummy);
	cpu->writeMem(0xFFFF,sec_slot_target);

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
			regs.BC.B.h--;
		}
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
			regs.BC.B.h--;
		}
	}

	motherboard->writeIO(0xA8, pri_slot, dummy);
	cpu->writeMem(0xFFFF,sec_slot);
	regs.AF.B.l &= (~Z80::C_FLAG);
	cpu->setCPURegs(regs);
}
void MSXDiskRomPatch::DSKCHG() const
{
	PRT_DEBUG("void MSXDiskRomPatch::DSKCHG() const");

	CPU* cpu = MSXCPU::instance()->getActiveCPU();
	CPU::CPURegs regs(cpu->getCPURegs());

	regs.IFF1 = true;

	// drive #, 0="A:", 1="B:", ..
	byte drive = regs.AF.B.h;

#ifdef DEBUG
	std::string driveletter("A");
	driveletter[0] += drive;
#endif
	PRT_DEBUG("    drive: " << driveletter << ":");

	if (drive >= MSXDiskRomPatch::LastDrive)
	{
		PRT_DEBUG("    Illegal Drive letter " << driveletter << ":");
		// illegal drive letter -> "Not Ready"
		regs.AF.w = 0x0201;
		cpu->setCPURegs(regs);
		return;
	}

	if (disk[drive] == 0)
	{
		PRT_DEBUG("    No Disk File For Drive " << driveletter << ":");
		// no disk file -> "Not Ready"
		regs.AF.w = 0x0201;
		cpu->setCPURegs(regs);
		return;
	}

	// Read media descriptor from first byte of FAT.
	byte media_descriptor = 0;
	disk[drive]->seek(1 * MSXDiskRomPatch::sector_size);
	disk[drive]->read(&media_descriptor, 1);
	if (disk[drive]->bad()) {
		PRT_DEBUG("    I/O error reading FAT");
		regs.AF.w = 0x0A01; // I/O error
		cpu->setCPURegs(regs);
		return;
	}

	regs.BC.B.h = media_descriptor;
	cpu->setCPURegs(regs);
	GETDPB();
	regs = cpu->getCPURegs();
	if (regs.AF.B.l & Z80::C_FLAG) {
		regs.AF.w = 0x0A01; // I/O error
		cpu->setCPURegs(regs);
		return;
	}

	regs.BC.B.h  = 0; // disk change unknown
	regs.AF.B.l &= (~Z80::C_FLAG);
	cpu->setCPURegs(regs);
}
void MSXDiskRomPatch::GETDPB() const
{
	PRT_DEBUG("void MSXDiskRomPatch::GETDPB() const");

	CPU* cpu = MSXCPU::instance()->getActiveCPU();
	CPU::CPURegs regs(cpu->getCPURegs());

	byte media_descriptor = regs.BC.B.h;
	// According to the docs there is also a media descriptor in C,
	// but the actual disk ROM code ignores it.
	int DPB_base_address = regs.HL.w;

	byte sect_per_fat;
	word maxclus;
	switch (media_descriptor) {
		case 0xF8:
			sect_per_fat = 2;
			maxclus = 355;
			break;
		case 0xF9:
			sect_per_fat = 3;
			maxclus = 714;
			break;
		case 0xFA:
			sect_per_fat = 1;
			maxclus = 316;
			break;
		case 0xFB:
			sect_per_fat = 2;
			maxclus = 635;
			break;
		default:
			PRT_DEBUG("    Illegal media_descriptor: " << std::hex
				<< static_cast<int>(media_descriptor) << std::dec);
			regs.AF.B.l |= (Z80::C_FLAG);
			cpu->setCPURegs(regs);
			return;
	}
	word sect_num_of_dir = 1 + sect_per_fat * 2;
	word sect_num_of_data = sect_num_of_dir + 7;

	// media type: passed by caller
	cpu->writeMem(DPB_base_address + 0x01, media_descriptor);
	// sector size: 512
	cpu->writeMem(DPB_base_address + 0x02, 0x00);
	cpu->writeMem(DPB_base_address + 0x03, 0x02);
	// directory mask/shift
	cpu->writeMem(DPB_base_address + 0x04, 0x0F);
	cpu->writeMem(DPB_base_address + 0x05, 4);
	// cluster mask/shift
	cpu->writeMem(DPB_base_address + 0x06, 0x01);
	cpu->writeMem(DPB_base_address + 0x07, 2);
	// sector # of first FAT
	cpu->writeMem(DPB_base_address + 0x08, 0x01);
	cpu->writeMem(DPB_base_address + 0x09, 0x00);
	// # of FATS
	cpu->writeMem(DPB_base_address + 0x0A, 2);
	// # of directory entries
	cpu->writeMem(DPB_base_address + 0x0B, 112);
	// sector # of first data sector
	cpu->writeMem(DPB_base_address + 0x0C, sect_num_of_data & 0xFF);
	cpu->writeMem(DPB_base_address + 0x0D, (sect_num_of_data>>8) & 0xFF);
	// # of clusters
	cpu->writeMem(DPB_base_address + 0x0E, maxclus & 0xFF);
	cpu->writeMem(DPB_base_address + 0x0F, (maxclus>>8) & 0xFF);
	// sectors per fat
	cpu->writeMem(DPB_base_address + 0x10, sect_per_fat);
	// sector # of dir
	cpu->writeMem(DPB_base_address + 0x11, sect_num_of_dir & 0xFF);
	cpu->writeMem(DPB_base_address + 0x12, (sect_num_of_dir>>8) & 0xFF);

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
