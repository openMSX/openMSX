// $Id$

#include "openmsx.hh"
#include "MSXDiskRomPatch.hh"
#include "MSXConfig.hh"
#include "CPU.hh"
#include "MSXCPUInterface.hh"
#include "DiskDrive.hh"
#include "FDCBackEnd.hh"


const int SECTOR_SIZE = 512;

const int A_PHYDIO = 0x4010;
const int A_DSKCHG = 0x4013;
const int A_GETDPB = 0x4016;
const int A_DSKFMT = 0x401C;
const int A_DRVOFF = 0x401F;

const byte MSXDiskRomPatch::bootSectorData[] = {
  0xeb, 0xfe, 0x90, 0x4e, 0x4d, 0x53, 0x20, 0x38, 0x32, 0x34, 0x35, 0x00, 0x02, 0x02, 0x01, 0x00,
  0x02, 0x70, 0x00, 0xa0, 0x05, 0xf9, 0x03, 0x00, 0x09, 0x00, 0x02, 0x00, 0x00, 0x00, 0xd0, 0xed,
  0x53, 0x59, 0xc0, 0x32, 0xc4, 0xc0, 0x36, 0x56, 0x23, 0x36, 0xc0, 0x31, 0x1f, 0xf5, 0x11, 0x79,
  0xc0, 0x0e, 0x0f, 0xcd, 0x7d, 0xf3, 0x3c, 0xca, 0x63, 0xc0, 0x11, 0x00, 0x01, 0x0e, 0x1a, 0xcd,
  0x7d, 0xf3, 0x21, 0x01, 0x00, 0x22, 0x87, 0xc0, 0x21, 0x00, 0x3f, 0x11, 0x79, 0xc0, 0x0e, 0x27,
  0xcd, 0x7d, 0xf3, 0xc3, 0x00, 0x01, 0x58, 0xc0, 0xcd, 0x00, 0x00, 0x79, 0xe6, 0xfe, 0xfe, 0x02,
  0xc2, 0x6a, 0xc0, 0x3a, 0xc4, 0xc0, 0xa7, 0xca, 0x22, 0x40, 0x11, 0x9e, 0xc0, 0x0e, 0x09, 0xcd,
  0x7d, 0xf3, 0x0e, 0x07, 0xcd, 0x7d, 0xf3, 0x18, 0xb2, 0x00, 0x4d, 0x53, 0x58, 0x44, 0x4f, 0x53,
  0x20, 0x20, 0x53, 0x59, 0x53, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x42, 0x6f,
  0x6f, 0x74, 0x20, 0x65, 0x72, 0x72, 0x6f, 0x72, 0x0d, 0x0a, 0x50, 0x72, 0x65, 0x73, 0x73, 0x20,
  0x61, 0x6e, 0x79, 0x20, 0x6b, 0x65, 0x79, 0x20, 0x66, 0x6f, 0x72, 0x20, 0x72, 0x65, 0x74, 0x72,
  0x79, 0x0d, 0x0a, 0x24
};

MSXDiskRomPatch::MSXDiskRomPatch(const EmuTime &time)
{
	// TODO make names configurable
	drives[0] = new DoubleSidedDrive("diska", time);
	drives[1] = new DoubleSidedDrive("diskb", time);
}

MSXDiskRomPatch::~MSXDiskRomPatch()
{
	delete drives[0];
	delete drives[1];
}

void MSXDiskRomPatch::patch(CPU::CPURegs& regs)
{
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
			// Patch not for diskrom
			break;
	}
}

void MSXDiskRomPatch::PHYDIO(CPU::CPURegs& regs)
{
	PRT_DEBUG("void MSXDiskRomPatch::PHYDIO() const");
	
	// TODO verify this
	// always return with interrupts enabled
	regs.ei();
	
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
	// TODO check this
	if (transferAddress + regs.BC.B.h*SECTOR_SIZE > 0x10000) {
		// read would overflow memory, adapt:
		regs.BC.B.h = (0x10000 - transferAddress) / SECTOR_SIZE;
	}

	// turn on RAM in all slots
	EmuTime dummy; // TODO
	MSXCPUInterface* cpuInterface = MSXCPUInterface::instance();
	int pri_slot = cpuInterface->readIO(0xA8, dummy);
	int sec_slot = cpuInterface->readMem(0xFFFF, dummy)^0xFF;
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
	cpuInterface->writeIO(0xA8, pri_slot_target, dummy);
	cpuInterface->writeMem(0xFFFF, sec_slot_target, dummy);

	byte buffer[SECTOR_SIZE];
	try {
		while (regs.BC.B.h) {	// num_sectors
			if (write) {
				for (int i=0; i<SECTOR_SIZE; i++) {
					buffer[i] = cpuInterface->readMem(transferAddress, dummy);
					transferAddress++;
				}
				drives[drive]->writeSector(buffer, sectorNumber);
			} else {
				drives[drive]->readSector(buffer, sectorNumber);
				for (int i=0; i<SECTOR_SIZE; i++) {
					cpuInterface->writeMem(transferAddress, buffer[i], dummy);
					transferAddress++;
				}
			}
			regs.BC.B.h--;
			sectorNumber++;
		}
		regs.AF.B.l &= ~CPU::C_FLAG;	// clear carry, OK
	} catch (NoSuchSectorException& e) {
		regs.AF.w = 0x0801;	// Record not found
	} catch (DiskIOErrorException& e) {
		regs.AF.w = 0x0A01;	// I/O error
	} catch (DriveEmptyException &e) {
		regs.AF.w = 0x0201;	// Not Ready
	} catch (WriteProtectedException &e) {
		regs.AF.w = 0x0001;	// Write Protected
	}

	// restore memory settings
	cpuInterface->writeIO(0xA8, pri_slot, dummy);
	cpuInterface->writeMem(0xFFFF,sec_slot, dummy);
}

void MSXDiskRomPatch::DSKCHG(CPU::CPURegs& regs)
{
	PRT_DEBUG("void MSXDiskRomPatch::DSKCHG() const");

	// TODO verify this
	// always return with interrupts enabled
	regs.ei();

	byte drive = regs.AF.B.h;	// drive #, 0="A:", 1="B:", ..

	if (drive >= LAST_DRIVE) {
		// illegal drive letter
		PRT_DEBUG("    Illegal Drive letter " << (int)drive);
		regs.AF.w = 0x0201;	// Not Ready
		return;
	}

	// Read media descriptor from first byte of FAT.
	byte buffer[SECTOR_SIZE];
	try {
		drives[drive]->readSector(buffer, 1);
	} catch (DiskIOErrorException& e) {
		PRT_DEBUG("    I/O error reading FAT");
		regs.AF.w = 0x0A01; // I/O error
		return;
	} catch (NoSuchSectorException& e) {
		PRT_DEBUG("    no sector 1, check your disk image");
		regs.AF.w = 0x0A01; // I/O error
		return;
	} catch (DriveEmptyException &e) {
		regs.AF.w = 0x0201;	// Not Ready
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

void MSXDiskRomPatch::GETDPB(CPU::CPURegs& regs)
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

	MSXCPUInterface* cpuInterface = MSXCPUInterface::instance();
	EmuTime dummy; // TODO
	// media type: passed by caller
	cpuInterface->writeMem(DPB_base_address + 0x01, media_descriptor, dummy);
	// sector size: 512
	cpuInterface->writeMem(DPB_base_address + 0x02, 0x00, dummy);
	cpuInterface->writeMem(DPB_base_address + 0x03, 0x02, dummy);
	// directory mask/shift
	cpuInterface->writeMem(DPB_base_address + 0x04, 0x0F, dummy);
	cpuInterface->writeMem(DPB_base_address + 0x05, 4,    dummy);
	// cluster mask/shift
	cpuInterface->writeMem(DPB_base_address + 0x06, 0x01, dummy);
	cpuInterface->writeMem(DPB_base_address + 0x07, 2,    dummy);
	// sector # of first FAT
	cpuInterface->writeMem(DPB_base_address + 0x08, 0x01, dummy);
	cpuInterface->writeMem(DPB_base_address + 0x09, 0x00, dummy);
	// # of FATS
	cpuInterface->writeMem(DPB_base_address + 0x0A, 2,    dummy);
	// # of directory entries
	cpuInterface->writeMem(DPB_base_address + 0x0B, 112,  dummy);
	// sector # of first data sector
	cpuInterface->writeMem(DPB_base_address + 0x0C, sect_num_of_data & 0xFF, dummy);
	cpuInterface->writeMem(DPB_base_address + 0x0D, (sect_num_of_data>>8) & 0xFF, dummy);
	// # of clusters
	cpuInterface->writeMem(DPB_base_address + 0x0E, maxclus & 0xFF, dummy);
	cpuInterface->writeMem(DPB_base_address + 0x0F, (maxclus>>8) & 0xFF, dummy);
	// sectors per fat
	cpuInterface->writeMem(DPB_base_address + 0x10, sect_per_fat, dummy);
	// sector # of dir
	cpuInterface->writeMem(DPB_base_address + 0x11, sect_num_of_dir & 0xFF, dummy);
	cpuInterface->writeMem(DPB_base_address + 0x12, (sect_num_of_dir>>8) & 0xFF, dummy);

	regs.AF.B.l &= ~CPU::C_FLAG;
}

void MSXDiskRomPatch::DSKFMT(CPU::CPURegs& regs)
{
	// Keep in mind following inputs
	// regs.AF.B.h  Specified choice (1-9)      
	// regs.DE.B.h  Drive number (0=A:)
	// regs.HL.w    Begin address of work area  
	// regs.BC.w    Length of work area
	// TODO: check how HL and BC are influenced!!
	regs.ei();

	PRT_DEBUG("sizeof(bootSectorData) :"<<sizeof(bootSectorData));
	PRT_DEBUG("regs.AF.B.h =" << (int)regs.AF.B.h);
	PRT_DEBUG("regs.AF.B.w =" << (int)regs.AF.w);

	byte drive = regs.DE.B.h;	// drive #, 0="A:", 1="B:", ..
	if (drive >= LAST_DRIVE) {
		// illegal drive letter
		PRT_DEBUG("    Illegal Drive letter " << (int)drive);
		regs.AF.w = 0x0C01;	// Bad parameter
		return;
	}
	byte index = regs.AF.B.h-1;
	if (index > 1) {
		// requested format not supported by DiskPatch
		PRT_DEBUG("    Format requested for not supported Media-ID");
		regs.AF.w = 0x0C01;	// Bad parameter
		return;
	}
	static struct {
		int NrOfSectors;
		byte NrOfHeads,DirEntries,SectorsPerTrack,SectorsPerFAT,SectorsPerCluster;
	}
	FormatInfo[2] = {
		{  720,1,112,9,2,2 }, // Normal 360kb single sided disk
		{ 1440,2,112,9,3,2 }, // Normal 720kb double sided disk
		/* these aren't supported for the moment
		   {  640,1,112,8,1,2 },
		   { 1280,2,112,8,2,2 },
		   {  360,1, 64,9,2,1 },
		   {  720,2,112,9,2,2 },
		   {  320,1, 64,8,1,1 },
		   {  640,2,112,8,1,2 }
		 */
	};

	// Fill bootblock with data:
	byte sectorData[SECTOR_SIZE];
	memset(sectorData, 0, SECTOR_SIZE);
	memcpy(sectorData, bootSectorData, sizeof(bootSectorData));
	memcpy(sectorData+3,"openMSXd",8);    // Manufacturer's ID
	sectorData[13] = FormatInfo[index].SectorsPerCluster;

	sectorData[17] = FormatInfo[index].DirEntries;
	sectorData[18] = 0;

	PRT_DEBUG("nr of sectors =" <<FormatInfo[index].NrOfSectors);
	sectorData[19] = FormatInfo[index].NrOfSectors&0xFF;
	sectorData[20] = (FormatInfo[index].NrOfSectors>>8)&0xFF;

	sectorData[21] = index+0xF8; //Media ID

	sectorData[22] = FormatInfo[index].SectorsPerFAT;
	sectorData[23] = 0;

	sectorData[24] = FormatInfo[index].SectorsPerTrack;
	sectorData[25] = 0;

	sectorData[26] = FormatInfo[index].NrOfHeads;
	sectorData[27] = 0;

	try {
		drives[drive]->writeSector(sectorData, 0);
		int Sector;
		byte J;

		/* Writing FATs: */
		for (Sector=1,J=0;J<2;J++) {
			sectorData[0] = index+0xF8;
			sectorData[1] = sectorData[2] = 0xFF;
			memset(sectorData+3,0,509);
			drives[drive]->writeSector(sectorData, Sector++);
			memset(sectorData,0,4); //clear first bytes again

			for (byte I=FormatInfo[index].SectorsPerFAT; I>1; I--)
				drives[drive]->writeSector(sectorData, Sector++);
		}

		// Directory size
		//J=FormatInfo[index].DirEntries/16;
		for (J=7,memset(sectorData,0x00,512);J;J--)
			drives[drive]->writeSector(sectorData, Sector++);

		// Data size
		J=FormatInfo[index].NrOfSectors-2*FormatInfo[index].SectorsPerFAT-8;
		memset(sectorData,0xE5,512);
		for (;J;J--)
			drives[drive]->writeSector(sectorData, Sector++);

		// Return success
		regs.AF.B.l &= ~CPU::C_FLAG;

	} catch (NoSuchSectorException& e) {
		regs.AF.w = 0x0801;	// Record not found
	} catch (DiskIOErrorException& e) {
		regs.AF.w = 0x0A01;	// I/O error
	} catch (DriveEmptyException &e) {
		regs.AF.w = 0x0201;	// Not Ready
	} catch (WriteProtectedException &e) {
		regs.AF.w = 0x0001;	// Write Protected
	}
}

void MSXDiskRomPatch::DRVOFF(CPU::CPURegs& regs)
{
	PRT_DEBUG("void MSXDiskRomPatch::DRVOFF() const [does nothing]");

	// do nothing
}
