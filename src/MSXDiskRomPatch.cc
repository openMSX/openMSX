// $Id$

#include "openmsx.hh"
#include "MSXDiskRomPatch.hh"
#include "MSXConfig.hh"
#include "CPU.hh"
#include "MSXCPU.hh"
#include "MSXMotherBoard.hh"
#include "ConsoleSource/ConsoleManager.hh"
#include "ConsoleSource/CommandController.hh"


const int MSXDiskRomPatch::A_PHYDIO = 0x4010;
const int MSXDiskRomPatch::A_DSKCHG = 0x4013;
const int MSXDiskRomPatch::A_GETDPB = 0x4016;
const int MSXDiskRomPatch::A_DSKFMT = 0x401C;
const int MSXDiskRomPatch::A_DRVOFF = 0x401F;
const byte MSXDiskRomPatch::bootSectorData[]={
  0xeb, 0xfe, 0x90, 0x4e, 0x4d, 0x53, 0x20, 0x38, 0x32, 0x34, 0x35,    0, 0x02, 0x02, 0x01,    0,
  0x02, 0x70,    0, 0xa0, 0x05, 0xf9, 0x03,    0, 0x09,    0, 0x02,    0,    0,    0, 0xd0, 0xed,
  0x53, 0x59, 0xc0, 0x32, 0xc4, 0xc0, 0x36, 0x56, 0x23, 0x36, 0xc0, 0x31, 0x1f, 0xf5, 0x11, 0x79,
  0xc0, 0x0e, 0x0f, 0xcd, 0x7d, 0xf3, 0x3c, 0xca, 0x63, 0xc0, 0x11,    0, 0x01, 0x0e, 0x1a, 0xcd,
  0x7d, 0xf3, 0x21, 0x01,    0, 0x22, 0x87, 0xc0, 0x21,    0, 0x3f, 0x11, 0x79, 0xc0, 0x0e, 0x27,
  0xcd, 0x7d, 0xf3, 0xc3,    0, 0x01, 0x58, 0xc0, 0xcd,    0,    0, 0x79, 0xe6, 0xfe, 0xfe, 0x02,
  0xc2, 0x6a, 0xc0, 0x3a, 0xc4, 0xc0, 0xa7, 0xca, 0x22, 0x40, 0x11, 0x9e, 0xc0, 0x0e, 0x09, 0xcd,
  0x7d, 0xf3, 0x0e, 0x07, 0xcd, 0x7d, 0xf3, 0x18, 0xb2,    0, 0x4d, 0x53, 0x58, 0x44, 0x4f, 0x53,
  0x20, 0x20, 0x53, 0x59, 0x53,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
     0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 0x42, 0x6f,
  0x6f, 0x74, 0x20, 0x65, 0x72, 0x72, 0x6f, 0x72, 0x0d, 0x0a, 0x50, 0x72, 0x65, 0x73, 0x73, 0x20,
  0x61, 0x6e, 0x79, 0x20, 0x6b, 0x65, 0x79, 0x20, 0x66, 0x6f, 0x72, 0x20, 0x72, 0x65, 0x74, 0x72,
  0x79, 0x0d, 0x0a, 0x24
};


MSXDiskRomPatch::DiskImage::DiskImage(std::string fileName,std::string defaultSize)
{
	//PRT_DEBUG("file = FileOpener::openFilePreferRW("<<fileName<<");");
	file = FileOpener::openFilePreferRW(fileName);
	if (!file->fail()){
	    file->seekg(0,std::ios::end);
	    if (!file->tellg()){
		    // For the moment I assume that
		    // if the file as zero length that is is just created
		    // Ofcourse it is possible that the file existed but
		    // that we do not have any rights on it whatsoever.
		    // So we test this.
		    file->put(0);
		    if (!file->fail()){
			    file->seekg(0,std::ios::beg);
			    PRT_DEBUG("Filling new file "<<fileName);
			    if (defaultSize.compare("360")==0){
			      for (long i=0;i<368640;i++) file->put(0);
			    } else {
			      for (long i=0;i<737280;i++) file->put(0);
			    }
			    //I thought about imediatly formatting the disk ?
			    //But I would need to alter the rest of the source to hard for the moment
			    //AND on a "real MSX" you should format a new disks also, so...
		    }
	    };
	    //file->seekg(0,std::ios::end);
	    nbSectors = file->tellg() / SECTOR_SIZE;
	}
}

MSXDiskRomPatch::DiskImage::~DiskImage()
{
	delete file;
}

void MSXDiskRomPatch::DiskImage::readSector(byte* to, int sector)
{
	if (sector >= nbSectors)
		throw NoSuchSectorException("No such sector");
	file->seekg(sector*SECTOR_SIZE, std::ios::beg);
	file->read(to, SECTOR_SIZE);
	if (file->bad())
		throw DiskIOErrorException("Disk I/O error");
}

void MSXDiskRomPatch::DiskImage::writeSector(const byte* from, int sector)
{
	if (sector >= nbSectors)
		throw NoSuchSectorException("No such sector");
	file->seekg(sector*SECTOR_SIZE, std::ios::beg);
	file->write(from, SECTOR_SIZE);
	if (file->bad())
		throw DiskIOErrorException("Disk I/O error");
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
	std::string defaultsize;
	std::string filename;
	PRT_INFO("Attaching drives...");
	for (int i=0; i<LAST_DRIVE; i++) {
		disk[i] = NULL;
		try {
			MSXConfig::Config *config = MSXConfig::Backend::instance()->getConfigById(name);
			filename = config->getParameter("filename");
			defaultsize = config->getParameter("defaultsize");
			disk[i] = new DiskImage(filename, defaultsize);
		} catch (MSXException& e) {
			PRT_DEBUG("MSXException "<< e.desc);
			PRT_INFO("Problems opening disk for drive "<<name[0xE] );
			delete disk[i];
			disk[i] = NULL;
		}
		// next drive letter
		name[0xE]++;
	}

	CommandController::instance()->registerCommand(*this, "disk");
	CommandController::instance()->registerCommand(*this, "diska");
	CommandController::instance()->registerCommand(*this, "diskb");
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
			//assert(false);
			//if left in this will break openMSX
			// with both tape and disk patch
			PRT_DEBUG("Patch not for diskrom");
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
	EmuTime dummy; // TODO
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
	} catch (NoSuchSectorException& e) {
		regs.AF.w = 0x0801;	// Record not found
	} catch (DiskIOErrorException& e) {
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
	} catch (DiskIOErrorException& e) {
		PRT_DEBUG("    I/O error reading FAT");
		regs.AF.w = 0x0A01; // I/O error
		return;
	} catch (NoSuchSectorException& e) {
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
	EmuTime dummy; // TODO
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
	// Keep in mind following inputs
	// regs.AF.B.h  Specified choice (1-9)      
	// regs.DE.B.h  Drive number (0=A:)
	// regs.HL.w    Begin address of work area  
	// regs.BC.w    Length of work area
	// TODO: check how HL and BC are influenced!!
	regs.IFF1 = regs.nextIFF1 = regs.IFF2 = true;	// EI

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
	if (disk[drive] == NULL) {
		// no disk file
		PRT_DEBUG("    No Disk File For Drive " << (int)drive);
		regs.AF.w = 0x0201;	// Not Ready
		return;
	}
    byte Index=regs.AF.B.h-1;
    if (Index>1) {
		// requested format not supported by DiskPatch
		PRT_DEBUG("    Format requested for not supported Media-ID");
		regs.AF.w = 0x0C01;	// Bad parameter
		return;
    }
  static struct
  { int NrOfSectors;byte NrOfHeads,DirEntries,SectorsPerTrack,SectorsPerFAT,SectorsPerCluster; }
  FormatInfo[2] =
  {
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
  
/* Fill bootblock with data: */
  byte *sectorData=new byte[512];
  memset(sectorData,0,512); //just to be sure (possible faulty C++ implemenatations :-)
  memcpy(sectorData,bootSectorData,sizeof(bootSectorData));
  memcpy(sectorData+3,"openMSXd",8);    /* Manufacturer's ID   */
  *(sectorData+13)=FormatInfo[Index].SectorsPerCluster;

  *(sectorData+17)=FormatInfo[Index].DirEntries;
  *(sectorData+18)=0;

  PRT_DEBUG("nr of sectors =" <<FormatInfo[Index].NrOfSectors);
  *(sectorData+19)=FormatInfo[Index].NrOfSectors&0xFF;
  *(sectorData+20)=(FormatInfo[Index].NrOfSectors>>8)&0xFF;

  *(sectorData+21)=Index+0xF8; //Media ID

  *(sectorData+22)=FormatInfo[Index].SectorsPerFAT;
  *(sectorData+23)=0;

  *(sectorData+24)=FormatInfo[Index].SectorsPerTrack;
  *(sectorData+25)=0;

  *(sectorData+26)=FormatInfo[Index].NrOfHeads;
  *(sectorData+27)=0;
  
  try {
    disk[drive]->writeSector(sectorData, 0);

    int Sector;
    byte J;

    /* Writing FATs: */
    for(Sector=1,J=0;J<2;J++)
    {
      sectorData[0]=Index+0xF8;
      sectorData[1]=sectorData[2]=0xFF;
      memset(sectorData+3,0,509);
      disk[drive]->writeSector(sectorData, Sector++);

      memset(sectorData,0,4); //clear first bytes again

      for(byte I=FormatInfo[Index].SectorsPerFAT;I>1;I--)
        disk[drive]->writeSector(sectorData, Sector++);
    }

    /* Directory size */
    //J=FormatInfo[Index].DirEntries/16;
    for(J=7,memset(sectorData,0x00,512);J;J--)
      disk[drive]->writeSector(sectorData, Sector++);


    /* Data size */
    J=FormatInfo[Index].NrOfSectors-2*FormatInfo[Index].SectorsPerFAT-8;
    memset(sectorData,0xE5,512);
    for(;J;J--)
      disk[drive]->writeSector(sectorData, Sector++);

    /* Return success      */
    regs.AF.B.l &= ~CPU::C_FLAG;

  } catch (NoSuchSectorException& e) {
    regs.AF.w = 0x0801;	// Record not found
  } catch (DiskIOErrorException& e) {
    regs.AF.w = 0x0A01;	// I/O error
  }

  delete[] sectorData;
  //assert(false);

}

void MSXDiskRomPatch::DRVOFF(CPU::CPURegs& regs) const
{
	PRT_DEBUG("void MSXDiskRomPatch::DRVOFF() const [does nothing]");

	// do nothing
}


void MSXDiskRomPatch::execute(const std::vector<std::string> &tokens)
{
	// TODO only works for 720Kb disks
	int drive = tokens[0][4];
	if (drive != 0)
		drive -= 'a';
	PRT_DEBUG("DiskCmd: drive "<<drive);
	if (tokens[1]=="eject") {
		ConsoleManager::instance()->print("Disk ejected");
		delete disk[drive];
		disk[drive] = NULL;
	} else {
		ConsoleManager::instance()->print("Changing disk");
		delete disk[drive];
		disk[drive] = NULL; // following might fail
		disk[drive] = new DiskImage(tokens[1], std::string("720"));
	}
}

void MSXDiskRomPatch::help(const std::vector<std::string> &tokens)
{
	ConsoleManager::instance()->print("disk eject      : remove disk from virtual drive");
	ConsoleManager::instance()->print("disk <filename> : change the disk file");
}

void MSXDiskRomPatch::tabCompletion(std::vector<std::string> &tokens)
{
	if (tokens.size()==2)
		CommandController::completeFileName(tokens[1]);
}
