// $Id$

#include "openmsx.hh"
#include "MSXDiskRomPatch.hh"
#include "msxconfig.hh"
#include "CPU.hh"
#include "MSXCPU.hh"
#include "MSXMotherBoard.hh"


const int MSXDiskRomPatch::A_PHYDIO = 0x4010;
const int MSXDiskRomPatch::A_DSKCHG = 0x4013;
const int MSXDiskRomPatch::A_GETDPB = 0x4016;
const int MSXDiskRomPatch::A_DSKFMT = 0x401C;
const int MSXDiskRomPatch::A_DRVOFF = 0x401F;


MSXDiskRomPatch::DiskImage::DiskImage(std::string fileName)
{
	file = FileOpener::openFilePreferRW(fileName);
	file->seekg(0,std::ios::end);
	nbSectors = file->tellg() / SECTOR_SIZE;
}

MSXDiskRomPatch::DiskImage::~DiskImage()
{
	delete file;
}

void MSXDiskRomPatch::DiskImage::readSector(byte* to, int sector)
{
	if (sector >= nbSectors)
		throw new NoSuchSectorException();
	file->seekg(sector*SECTOR_SIZE, std::ios::beg);
	file->read(to, SECTOR_SIZE);
	if (file->bad())
		throw new DiskIOErrorException();
}

void MSXDiskRomPatch::DiskImage::writeSector(const byte* from, int sector)
{
	if (sector >= nbSectors)
		throw new NoSuchSectorException();
	file->seekg(sector*SECTOR_SIZE, std::ios::beg);
	file->write(from, SECTOR_SIZE);
	if (file->bad())
		throw new DiskIOErrorException();
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
	for (int i=0; i<LAST_DRIVE; i++) {
		disk[i] = NULL;
		try {
			MSXConfig::Config *config = MSXConfig::instance()->getConfigById(name);
			filename = config->getParameter("filename");
			disk[i] = new DiskImage(filename);
		} catch (MSXException e) {
			PRT_DEBUG("void MSXDiskRomPatch::MSXDiskRomPatch() disk exception for disk " << i << " patch: " << name << " filename: " << filename);
			delete disk[i];
			disk[i] = NULL;
		}
		// next drive letter
		name[0xE]++;
	}
}

MSXDiskRomPatch::~MSXDiskRomPatch()
{
	for (int i=0; i<LAST_DRIVE; i++) {
		if (disk[i] != NULL) delete disk[i];
	}
}

void MSXDiskRomPatch::patch() const
{
	CPU::CPURegs& regs = MSXCPU::instance()->getCPURegs();
	switch (regs.PC.w-2) {
		case A_PHYDIO:
			PHYDIO(regs);
			break;
		case A_DSKCHG:
			DSKCHG(regs);
			break;
		case A_GETDPB:
			GETDPB(regs);
			break;
		case A_DSKFMT:
			DSKFMT(regs);
			break;
		case A_DRVOFF:
			DRVOFF(regs);
			break;
		default:
			assert(false);
	}
}

void MSXDiskRomPatch::PHYDIO(CPU::CPURegs& regs) const
{
	PRT_DEBUG("void MSXDiskRomPatch::PHYDIO() const");
	
	// TODO verify this
	// always return with interrupts enabled
	regs.IFF1 = regs.nextIFF1 = regs.IFF2 = true;	// EI
	
	byte drive = regs.AF.B.h;			// drive #, 0="A:", 1="B:", ..
	//regs.BC.B.h;					// nb of sectors to read/write
	word sectorNumber = regs.DE.w;			// logical sector number
	word transferAddress = regs.HL.w;		// transfer address
	bool write = (regs.AF.B.l & CPU::C_FLAG);	// read or write, Carry==write
	
	PRT_DEBUG("    drive: " << (int)drive);
	PRT_DEBUG("    num_sectors: " << (int)regs.BC.B.h);
	PRT_DEBUG("    sector_number: " << (int)sectorNumber);
	PRT_DEBUG("    transfer_address: 0x" << std::hex << transferAddress << std::dec);
	PRT_DEBUG("    write/read: " << std::string(write?"write":"read"));

	if (drive >= LAST_DRIVE) {
		// illegal drive letter
		PRT_DEBUG("    Illegal Drive letter ");
		regs.AF.w = 0x0201;	// Not Ready
		return;
	}
	if (disk[drive] == NULL) {
		// no disk file
		regs.AF.w = 0x0201;	// Not Ready
		return;
	}
	// TODO check this
	if (transferAddress + regs.BC.B.h*SECTOR_SIZE > 0x10000) {
		// read would overflow memory, adapt:
		regs.BC.B.h = (0x10000 - transferAddress) / SECTOR_SIZE;
	}

	// turn on RAM in all slots
	EmuTime dummy(0);
	MSXMotherBoard* motherboard = MSXMotherBoard::instance();
	int pri_slot = motherboard->readIO(0xA8, dummy);
	int sec_slot = motherboard->readMem(0xFFFF, dummy)^0xFF;
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
	motherboard->writeMem(0xFFFF,sec_slot_target, dummy);

	byte buffer[SECTOR_SIZE];
	try {
		while (regs.BC.B.h) {	// num_sectors
			if (write) {
				for (int i=0; i<SECTOR_SIZE; i++) {
					buffer[i] = motherboard->readMem(transferAddress, dummy);
					transferAddress++;
				}
				disk[drive]->writeSector(buffer, sectorNumber);
			} else {
				disk[drive]->readSector(buffer, sectorNumber);
				for (int i=0; i<SECTOR_SIZE; i++) {
					motherboard->writeMem(transferAddress, buffer[i], dummy);
					transferAddress++;
				}
			}
			regs.BC.B.h--;
			sectorNumber++;
		}
		regs.AF.B.l &= ~CPU::C_FLAG;	// clear carry, OK
	} catch (NoSuchSectorException* e) {
		regs.AF.w = 0x0801;	// Record not found
	} catch (DiskIOErrorException* e) {
		regs.AF.w = 0x0A01;	// I/O error
	}

	// restore memory settings
	motherboard->writeIO(0xA8, pri_slot, dummy);
	motherboard->writeMem(0xFFFF,sec_slot, dummy);
}

void MSXDiskRomPatch::DSKCHG(CPU::CPURegs& regs) const
{
	PRT_DEBUG("void MSXDiskRomPatch::DSKCHG() const");

	// TODO verify this
	// always return with interrupts enabled
	regs.IFF1 = regs.nextIFF1 = regs.IFF2 = true;	// EI

	byte drive = regs.AF.B.h;	// drive #, 0="A:", 1="B:", ..

	if (drive >= LAST_DRIVE) {
		// illegal drive letter
		PRT_DEBUG("    Illegal Drive letter " << (int)drive);
		regs.AF.w = 0x0201;	// Not Ready
		return;
	}
	if (disk[drive] == NULL) {
		// no disk file
		PRT_DEBUG("    No Disk File For Drive " << (int)drive);
		regs.AF.w = 0x0201;	// Not Ready
		return;
	}

	// Read media descriptor from first byte of FAT.
	byte buffer[SECTOR_SIZE];
	try {
		disk[drive]->readSector(buffer, 1);
	} catch (DiskIOErrorException* e) {
		PRT_DEBUG("    I/O error reading FAT");
		regs.AF.w = 0x0A01; // I/O error
		return;
	} catch (NoSuchSectorException* e) {
		PRT_DEBUG("    no sector 1, check your disk image");
		regs.AF.w = 0x0A01; // I/O error
		return;
	}
	regs.BC.B.h = buffer[0];
	GETDPB(regs);
	if (regs.AF.B.l & CPU::C_FLAG) {
		regs.AF.w = 0x0A01; // I/O error
		return;
	}

	regs.BC.B.h = 0; // disk change unknown
	regs.AF.B.l &= ~CPU::C_FLAG;
}

void MSXDiskRomPatch::GETDPB(CPU::CPURegs& regs) const
{
	PRT_DEBUG("void MSXDiskRomPatch::GETDPB() const");

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
			regs.AF.B.l |= (CPU::C_FLAG);
			return;
	}
	word sect_num_of_dir = 1 + sect_per_fat * 2;
	word sect_num_of_data = sect_num_of_dir + 7;

	MSXMotherBoard* motherboard = MSXMotherBoard::instance();
	EmuTime dummy(0);
	// media type: passed by caller
	motherboard->writeMem(DPB_base_address + 0x01, media_descriptor, dummy);
	// sector size: 512
	motherboard->writeMem(DPB_base_address + 0x02, 0x00, dummy);
	motherboard->writeMem(DPB_base_address + 0x03, 0x02, dummy);
	// directory mask/shift
	motherboard->writeMem(DPB_base_address + 0x04, 0x0F, dummy);
	motherboard->writeMem(DPB_base_address + 0x05, 4,    dummy);
	// cluster mask/shift
	motherboard->writeMem(DPB_base_address + 0x06, 0x01, dummy);
	motherboard->writeMem(DPB_base_address + 0x07, 2,    dummy);
	// sector # of first FAT
	motherboard->writeMem(DPB_base_address + 0x08, 0x01, dummy);
	motherboard->writeMem(DPB_base_address + 0x09, 0x00, dummy);
	// # of FATS
	motherboard->writeMem(DPB_base_address + 0x0A, 2,    dummy);
	// # of directory entries
	motherboard->writeMem(DPB_base_address + 0x0B, 112,  dummy);
	// sector # of first data sector
	motherboard->writeMem(DPB_base_address + 0x0C, sect_num_of_data & 0xFF, dummy);
	motherboard->writeMem(DPB_base_address + 0x0D, (sect_num_of_data>>8) & 0xFF, dummy);
	// # of clusters
	motherboard->writeMem(DPB_base_address + 0x0E, maxclus & 0xFF, dummy);
	motherboard->writeMem(DPB_base_address + 0x0F, (maxclus>>8) & 0xFF, dummy);
	// sectors per fat
	motherboard->writeMem(DPB_base_address + 0x10, sect_per_fat, dummy);
	// sector # of dir
	motherboard->writeMem(DPB_base_address + 0x11, sect_num_of_dir & 0xFF, dummy);
	motherboard->writeMem(DPB_base_address + 0x12, (sect_num_of_dir>>8) & 0xFF, dummy);

	regs.AF.B.l &= ~CPU::C_FLAG;
}

void MSXDiskRomPatch::DSKFMT(CPU::CPURegs& regs) const
{
	assert(false);
	// TODO XXX
}

void MSXDiskRomPatch::DRVOFF(CPU::CPURegs& regs) const
{
	PRT_DEBUG("void MSXDiskRomPatch::DRVOFF() const [does nothing]");

	// do nothing
}
