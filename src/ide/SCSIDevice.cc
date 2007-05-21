// $Id$
/* Ported from:
** Source: /cvsroot/bluemsx/blueMSX/Src/IoDevice/ScsiDevice.c,v
** Revision: 1.10
** Date: 2007-05-21 21:38:29 +0200 (Mon, 21 May 2007)
**
** More info: http://www.bluemsx.com
**
** Copyright (C) 2003-2007 Daniel Vik, white cat
*/

/*
 * Notes:
 *  It follows the SCSI1(CCS) standard or the SCSI2 standard.
 *  Only the direct access device is supported now.
 *  Message system might be imperfect.
 */

#include "SCSIDevice.hh"
//TODO: #include "ArchCdrom.h"
#include "SCSI.hh"
//TODO: #include "Led.h"
#include "File.hh"
#include "FileException.hh"
#include <cstring>

#include <iostream>
#undef PRT_DEBUG
#define  PRT_DEBUG(mes)                          \
        do {                                    \
                std::cout << mes << std::endl;  \
        } while (0)

using std::string;

#define USE_SPECIALCOMMAND

// Medium type (value like LS-120)
#define MT_UNKNOWN      0x00
#define MT_2DD_UN       0x10
#define MT_2DD          0x11
#define MT_2HD_UN       0x20
#define MT_2HD_12_98    0x22
#define MT_2HD_12       0x23
#define MT_2HD_144      0x24
#define MT_LS120        0x31
#define MT_NO_DISK      0x70
#define MT_DOOR_OPEN    0x71
#define MT_FMT_ERROR    0x72

namespace openmsx {

static const byte inqdata[36] = {
       0,   // bit5-0 device type code.
       0,   // bit7 = 1 removable device
       2,   // bit7,6 ISO version. bit5,4,3 ECMA version.
            // bit2,1,0 ANSI Version (001=SCSI1, 010=SCSI2)
       2,   // bit7 AENC. bit6 TrmIOP.
            // bit3-0 Response Data Format. (0000=SCSI1, 0001=CCS, 0010=SCSI2)
      51,   // addtional length
       0, 0,// reserved
       0,   // bit7 RelAdr, bit6 WBus32, bit5 Wbus16, bit4 Sync, bit3 Linked,
            // bit2 reseved bit1 CmdQue, bit0 SftRe
     'o', 'p', 'e', 'n', 'M', 'S', 'X', ' ',    // vendor ID (8bytes)
     'S', 'C', 'S', 'I', '2', ' ', 'H', 'a',    // product ID (16bytes)
     'r', 'd', 'd', 'i', 's', 'k', ' ', ' ',
     '0', '1', '0', 'a' };                  // product version (ASCII 4bytes)

static const char sdt_name[10][10 + 1] =
{
	{"Tape      "},
	{"Printer   "},
	{"Processor "},
	{"WORM      "},
	{"CD-ROM    "},
	{"Scanner   "},
	{"MO        "},
	{"Jukebox   "},
	{"COMM      "},
	{"???       "}
};

// for FDSFORM.COM
static const char fds120[28 + 1]  = "IODATA  LS-120 COSM     0001";

SCSIDevice::SCSIDevice(int scsiId_, byte* buf, char* name_,
                       int type, int mode_/*TODO:, CdromXferCompCb xferCompCb*/) 
	  : scsiId(scsiId_)
	  , deviceType(type)
	  , mode(mode_)
	  , buffer(buf)
	  , productName(name_)
{
	enabled       = true;
	sectorSize    = 512;
	/* TODO: MOVE CD-ROM STUFF TO SEPARATE CLASS
	   cdrom         = NULL;
	
	   disk.fileName[0] = 0;
	   disk.fileNameInZip[0] = 0;
	   disk.directory[0] = 0;
	   disk.extensionFilter = 0;
	   disk.type = 0;

	if (type == SDT_CDROM) {
		sectorSize = 2048;
		  cdrom = archCdromCreate(xferCompCb, ref);
		  if (cdrom == NULL) {
		  enabled = false;
		  }
	}*/

	string filename = "/tmp/SCSIdisk.dsk"; // TODO: don't hardcode this
	try {
		file.reset(new File(filename));
	} catch (FileException& e) {
		// image didn't exist yet, create new
		file.reset(new File(filename, File::CREATE));
		file->truncate(100 * 1024 * 1024); // TODO: don't hardcode this
	}

	name = "sda";

	reset();
}

SCSIDevice::~SCSIDevice()
{
	PRT_DEBUG("hdd close for hdd " << scsiId);
	/* TODO:    if (deviceType == SDT_CDROM) {
	   archCdromDestroy(cdrom);
	   } */
}

void SCSIDevice::reset()
{
	/* TODO:    if (deviceType == SDT_CDROM) {
	   archCdromHwReset(cdrom);
	   }*/
	changed       = false;
	keycode       = 0;
	currentSector        = 0;
	currentLength        = 0;
	motor         = true;
	changeCheck2  = true;    // the first use always
	unitAttention         = (mode & MODE_UNITATTENTION);

	//TODO:    disk = propGetGlobalProperties()->media.disks[diskId];
	/* TODO:    inserted   = (strlen(disk.fileName) > 0);
	   if (inserted) {
	   PRT_DEBUG("hdd %d: \n", scsiId);
	   PRT_DEBUG("filename: %s\n", disk.fileName);
	   PRT_DEBUG("     zip: %s\n", disk.fileNameInZip);
	   }
	   else if ((mode & MODE_NOVAXIS) && deviceType != SDT_CDROM) {
	   enabled = false;
	   }*/
	// FOR NOW:
	inserted = true;
}

void SCSIDevice::busReset()
{
	PRT_DEBUG("SCSI: bus reset on " << scsiId);
	keycode = 0;
	unitAttention = (mode & MODE_UNITATTENTION);
	/* TODO: if (deviceType == SDT_CDROM) {
	   archCdromBusReset(cdrom);
	   }*/
}

void SCSIDevice::disconnect()
{
	/* TODO
	   if (deviceType != SDT_CDROM) {
	   ledSetHd(0);
	   } else {
	   archCdromDisconnect(cdrom);
	   }*/
}

void SCSIDevice::enable(bool enable)
{
	enabled = enable;
}

// Check the initiator in the call origin.
bool SCSIDevice::isSelected()
{
	lun = 0;
	if (mode & MODE_REMOVABLE) {
		if (!enabled &&
				(mode & MODE_NOVAXIS) && deviceType != SDT_CDROM) {
			//TODO:            enabled = diskPresent(diskId) ? 1 : 0;
			// FOR NOW:
			enabled = true;
		}
		return enabled;
	}
	return enabled /* TODO: && diskPresent(diskId) */;
}

bool SCSIDevice::getReady()
{
	/* TODO:    if (diskPresent(diskId)) { */
	   return true;
	  /* } */
	// TODO: keycode = SENSE_MEDIUM_NOT_PRESENT;
	// TODO: return false;
}

bool SCSIDevice::diskChanged()
{
	/* TODO:
	   FileProperties* pDisk;
	   bool tmpChanged = diskChanged(diskId);

	   if (tmpChanged) {
	   motor = true;
	   pDisk = &propGetGlobalProperties()->media.disks[diskId];

	   if (changeCheck2) {
	   changeCheck2 = false;
	   if (inserted &&
	   (strcmp(disk.fileName, pDisk->fileName) == 0) &&
	   (strcmp(disk.fileNameInZip, pDisk->fileNameInZip) == 0)) {
	   PRT_DEBUG("Disk change invalidity\n\n");
	   return 0;
	   }
	   }
	   changed  = true;
	   disk = *pDisk;
	   inserted = true;

	   PRT_DEBUG("hdd %d: disk change\n", scsiId);
	   PRT_DEBUG("filename: %s\n", disk.fileName);
	   PRT_DEBUG("     zip: %s\n", disk.fileNameInZip);
	   } else {
	   if (inserted & !diskPresent(diskId)) {
	   inserted   = false;
	   motor      = false;
	   changed    = true;
	   tmpChanged = true;
	   }
	   }
	   */
	bool tmpChanged = false; // REMOVE THIS LINE WHEN THE TODO IS DONE!!!!!!!!!!!!!!!!!!!

	if (tmpChanged && (mode & MODE_UNITATTENTION)) {
		unitAttention = true;
	}
	return tmpChanged;
}

void SCSIDevice::testUnitReady()
{
	if ((mode & MODE_NOVAXIS) == 0) {
		if (getReady() && changed && (mode & MODE_MEGASCSI)) {
			// Disk change is surely sent for the driver of MEGA-SCSI.
			keycode = SENSE_POWER_ON;
		}
	}
	changed = false;
}

//TODO: lots of stuff in this method
void SCSIDevice::startStopUnit()
{
	//TODO:    FileProperties* disk = &propGetGlobalProperties()->media.disks[diskId];

	switch (cdb[4]) {
		case 2:
        // Eject
//TODO        if (diskPresent(diskId)) {
//            disk->fileName[0] = 0;
//            disk->fileNameInZip[0] = 0;
//            updateExtendedDiskName(diskId, disk->fileName, disk->fileNameInZip);
//            boardChangeDiskette(diskId, NULL, NULL);
            PRT_DEBUG("eject hdd " << scsiId);
//        } 
			break;
		case 3:
        // Insert
//TODO        if (!diskPresent(diskId)) {
//            *disk = disk;
//            updateExtendedDiskName(diskId, disk->fileName, disk->fileNameInZip);
//            boardChangeDiskette(diskId, disk->fileName, disk->fileNameInZip);
            PRT_DEBUG("insert hdd " << scsiId);
//        } 
			break;
	}
	motor = cdb[4] & 1;
	PRT_DEBUG("hdd " << scsiId << " motor: " << motor);
}

//TODO also a lot to do here, lots of interfacing with a SectorAccessibleDisk
int SCSIDevice::inquiry()
{
	int total       = getNbSectors();
	int length      = currentLength;
	byte type      = (byte)(deviceType & 0xff);
	byte removable;
	
	bool fdsmode = (mode & MODE_FDS120) && (total > 0) && (total <= 2880);

	if (length == 0) return 0;

	if (fdsmode) {
		memcpy(buffer + 2, inqdata + 2, 6);
		memcpy(buffer + 8, fds120, 28);
		removable = 0x80;
		if (type != SDT_DirectAccess) {
			type = SDT_Processor;
		}
	} else {
		memcpy(buffer + 2, inqdata + 2, 34);
		removable = (mode & MODE_REMOVABLE) ? 0x80 : 0;

		if (productName == NULL) {
			int dt = deviceType;
			if (dt != SDT_DirectAccess) {
				if (dt > SDT_Communications) {
					dt = SDT_Communications + 1;
				}
				--dt;
				memcpy(buffer + 22, sdt_name[dt], 10);
			}
		} else {
			memcpy(buffer + 16, productName, 16);
		}
	}

	buffer[0] = type;
	buffer[1] = removable;

	if (!(mode & BIT_SCSI2)) {
		buffer[2]  = 1;
		buffer[3]  = 1;
		if (!fdsmode) buffer[20] = '1';
	} else {
		if (mode & BIT_SCSI3) {
			buffer[2]  = 5;
			if (!fdsmode) buffer[20] = '3';
		}
	}

	if ((mode & MODE_CHECK2) && !fdsmode) {
		buffer[35] = 'A';
	}
	if (mode & BIT_SCSI3) {
		if (length > 96) length = 96;
		buffer[4] = 91;
		if (length > 56) {
			memset(buffer + 56, 0, 40);
			buffer[58] = 0x03;
			buffer[60] = 0x01;
			buffer[61] = 0x80;
		}
	} else {
		if (length > 56) length = 56;
	}

	const char* fileName;
	if (length > 36){
		buffer += 36;
		memset(buffer, ' ', 20);
		/*TODO        fileName = strlen(disk.fileNameInZip) ? disk.fileNameInZip : disk.fileName;
		  fileName = stripPath(fileName);*/
		fileName = file->getURL().c_str();// TODO: strip path
		for (int i = 0; i < 20; ++i) {
			if (*fileName == 0) {
				break;
			}
			*buffer = *fileName;
			++buffer;
			++fileName;
		} 
	}
	return length;
}

int SCSIDevice::modeSense()
{
	int length  = currentLength;

	if ((length > 0) && (cdb[2] == 3)) {
		int total       = getNbSectors();
		int media       = MT_UNKNOWN;
		int sectors     = 64;
		int blockLength = sectorSize >> 8;
		int tracks      = 8;
		int size        = 4 + 24;
		int removable   = mode & MODE_REMOVABLE ? 0xa0 : 0x80;

		memset(buffer + 2, 0, 34);

		if (total == 0) {
			media = MT_NO_DISK;
		} else {
			if (mode & MODE_FDS120) {
				if (total == 1440) {
					media       = MT_2DD;
					sectors     = 9;
					blockLength = 2048 >> 8;            // FDS-120 value
					tracks      = 160;
					removable   = 0xa0;
				} else {
					if (total == 2880) {
						media       = MT_2HD_144;
						sectors     = 18;
						blockLength = 2048 >> 8;
						tracks      = 160;
						removable   = 0xa0;
					}
				}
			}
		}

		// Mode Parameter Header 4bytes
		buffer[1] = media;              // Medium Type
		buffer[3] = 8;                  // block descripter length
		buffer   += 4;

		// Disable Block Descriptor check
		if (!(cdb[1] & 0x08)) {
			// Block Descriptor 8bytes
			buffer[1]  = (byte)((total >> 16) & 0xff); // 1..3 Number of Blocks
			buffer[2]  = (byte)((total >>  8) & 0xff);
			buffer[3]  = (byte)(total & 0xff);
			buffer[6]  = (byte)(blockLength & 0xff);   // 5..7 Block Length in Bytes
			buffer    += 8;
			size      += 8;
		}

		// Format Device Page 24bytes
		buffer[0]  = 3;                 //     0 Page
		buffer[1]  = 0x16;              //     1 Page Length
		buffer[3]  = (byte)tracks;     //  2, 3 Tracks per Zone
		buffer[11] = (byte)sectors;    // 10,11 Sectors per Track
		buffer[12] = (byte)blockLength;// 12,13 Data Bytes per Physical Sector
		buffer[20] = removable;         // 20    bit7 Soft Sector bit5 Removable

		buffer[0] = size - 1;     // sense data length

		if (length > size) {
			length = size;
		}
		return length;
	}
	keycode = SENSE_INVALID_COMMAND_CODE;
	return 0;
}

int SCSIDevice::requestSense()
{
	int tmpKeycode;
	int length = currentLength;

	if (unitAttention) {
		unitAttention = false;
		tmpKeycode = SENSE_POWER_ON;
	} else {
		tmpKeycode = keycode;
	}

	PRT_DEBUG("Request Sense: keycode = " << tmpKeycode);
	keycode = SENSE_NO_SENSE;

	memset(buffer + 1, 0, 17);
	if (length == 0) {
		if (mode & BIT_SCSI2) {
			return 0;
		}
		buffer[0]    = (byte)((tmpKeycode >> 8) & 0xff);  // Sense code
		length = 4;
	} else {
		buffer[0]  = 0x70;
		buffer[2]  = (byte)((tmpKeycode >> 16) & 0xff);   // Sense key
		buffer[7]  = 10;                                // Additional sense length
		buffer[12] = (byte)((tmpKeycode >> 8) & 0xff);    // Additional sense code
		buffer[13] = (byte)(tmpKeycode & 0xff);           // Additional sense code qualifier
		if (length > 18) length = 18;
	}
	return length;
}

bool SCSIDevice::checkReadOnly()
{
	if (file->isReadOnly()) {
		keycode = SENSE_WRITE_PROTECT;
		return true;
	}
	return false;
}

int SCSIDevice::readCapacity()
{
	unsigned block = getNbSectors();

	if (block == 0) {
		keycode = SENSE_MEDIUM_NOT_PRESENT;
		PRT_DEBUG("hdd " << scsiId << ": drive not ready");
		return 0;
	}

	PRT_DEBUG("total block: " << (unsigned int)block);

	--block;
	buffer[0] = (byte)((block >> 24) & 0xff);
	buffer[1] = (byte)((block >> 16) & 0xff);
	buffer[2] = (byte)((block >>  8) & 0xff);
	buffer[3] = (byte)(block & 0xff);
	buffer[4] = 0;
	buffer[5] = 0;
	buffer[6] = (byte)((sectorSize >> 8) & 0xff);
	buffer[7] = 0;

	return 8;
}

/*
// This routine demands 'BUFFER_SIZE' by 1KB or more.
int SCSIDevice::openMSX()
{
	int length;
	const char* fileName;
	const char scsicat[16 + 1] = "SCSI-CATopenMSX ";
	int cmd;

	PRT_DEBUG("SCSI-CAT " << scsiId);
	cmd = (cdb[4] << 8) + cdb[5];

	if (cmd == 0) {
		// revision
		memcpy(buffer , scsicat, 16);       // SCSI-CAT(8) + emulator name(8)
		buffer[16] = (byte)(deviceType & 0xff);  // device type
		buffer[17] = mode & MODE_REMOVABLE ? 0x80 : 0;    // removable device
		buffer[18] = 0;                     // revision MSB
		buffer[19] = 1;                     // LSB
		return 20;
	}
	if (cmd == 1) {
		// inserted
		//TODO:        buffer[0] = diskPresent(diskId) ? 1 : 0;
		//FOR NOW:
		buffer[0] = 1;
		return 1;
	}
	if (cmd < 6) {
        // file info
//TODO:        disk = &disk;
//        fileName = (cmd < 4) ? disk->fileName : disk->fileNameInZip;
//       if ((cmd & 1) == 0) {
//            fileName = stripPath(fileName);
//        }
//        length = strlen(fileName);
//        buffer[0] = (byte)((length >> 8) & 0xff);
//        buffer[1] = (byte)(length & 0xff);
//        strcpy(buffer + 2, fileName);
//        PRT_DEBUG("file info:\n" << *(buffer + 2));
 //       return length + 3;  // + \0 
	}
	keycode = SENSE_INVALID_COMMAND_CODE;
	return 0;
}
*/

bool SCSIDevice::checkAddress()
{
	int total = getNbSectors(); 
	if (total == 0) {
		keycode = SENSE_MEDIUM_NOT_PRESENT;
		PRT_DEBUG("hdd " << scsiId << ": drive not ready");
		return false;
	}

	if ((currentSector >= 0) && (currentLength > 0) &&
			(currentSector + currentLength <= total)) {
		return true;
	}
	PRT_DEBUG("hdd " << scsiId << ": IllegalBlockAddress");
	keycode = SENSE_ILLEGAL_BLOCK_ADDRESS;
	return false;
}

// Execute scsiDeviceCheckAddress previously.
int SCSIDevice::readSector(int* blocks)
{
	int counter;
	int numSectors;

	//TODO:    ledSetHd(1);
	if (currentLength >= BUFFER_BLOCK_SIZE) {
		numSectors  = BUFFER_BLOCK_SIZE;
		counter     = BUFFER_SIZE;
	} else {
		numSectors  = currentLength;
		counter     = currentLength * sectorSize;
	}

	PRT_DEBUG("hdd#" << scsiId << " read sector: " << currentSector << " " << numSectors);
	try {
		//TODO: somehow map this to SectorAccessibleDisk::readLogicalSector?
		file->seek(sectorSize * currentSector);
		file->read(buffer, sectorSize * numSectors);
		currentSector += numSectors;
		currentLength -= numSectors;
		*blocks = currentLength;
		return counter;
	} catch (FileException& e) {
		*blocks = 0;
		keycode = SENSE_UNRECOVERED_READ_ERROR;
		return 0;
	}
}

int SCSIDevice::dataIn(int* blocks)
{
	if (cdb[0] == SCSIOP_READ10) {
		int counter = readSector(blocks);
		if (counter) {
			return counter;
		}
	}
	PRT_DEBUG("dataIn error " << cdb[0]);
	*blocks = 0;
	return 0;
}

// Execute scsiDeviceCheckAddress and scsiDeviceCheckReadOnly previously.
int SCSIDevice::writeSector(int* blocks)
{
	int numSectors;

	//TODO:    ledSetHd(1);
	if (currentLength >= BUFFER_BLOCK_SIZE) {
		numSectors  = BUFFER_BLOCK_SIZE;
	} else {
		numSectors  = currentLength;
	}

	PRT_DEBUG("hdd#" << scsiId << " write sector: " << currentSector << " " << numSectors);
		//TODO: somehow map this to SectorAccessibleDisk::writeLogicalSector?
	try {
		file->seek(sectorSize * currentSector);
		file->write(buffer, sectorSize * numSectors);
		currentSector += numSectors;
		currentLength -= numSectors;

		int counter;
		if (currentLength >= BUFFER_BLOCK_SIZE) {
			*blocks = currentLength - BUFFER_BLOCK_SIZE;
			counter = BUFFER_SIZE;
		} else {
			*blocks = 0;
			counter = currentLength * sectorSize;
		}
		return counter;
	} catch (FileException& e) {
		keycode = SENSE_WRITE_FAULT;
		*blocks = 0;
		return 0;
	}
}

int SCSIDevice::dataOut(int* blocks)
{
	if (cdb[0] == SCSIOP_WRITE10) {
		return writeSector(blocks);
	}
	PRT_DEBUG("dataOut error " << (int)cdb[0]);
	*blocks = 0;
	return 0;
}

//  MBR erase only
void SCSIDevice::formatUnit()
{
	if (getReady() && !checkReadOnly()) {
		memset(buffer, 0, sectorSize);
		try {
			file->seek(0);
			file->write(buffer, sectorSize);
			unitAttention = true;
			changed = true;
		} catch (FileException& e) {
			keycode = SENSE_WRITE_FAULT;
		}
	}
}

byte SCSIDevice::getStatusCode()
{
	byte result;
//TODO	if (deviceType != SDT_CDROM) {
		result = keycode ? SCSIST_CHECK_CONDITION : SCSIST_GOOD;
//	} else {
//TODO          result = archCdromGetStatusCode(cdrom);
//	}
	PRT_DEBUG("SCSI status code: \n" << (int)result);
	return result;
}

int SCSIDevice::executeCmd(byte* cdb_, SCSI_PHASE* phase, int* blocks)
{
	PRT_DEBUG("SCSI Command: " << (int)cdb[0]);
	memcpy(cdb, cdb_, 12);
	message = 0;
	*phase = Status;
	*blocks = 0;

	/* TODO
	if (deviceType == SDT_CDROM) {
		int retval;
		keycode = SENSE_NO_SENSE;
		*phase = Execute;
		retval = archCdromExecCmd(cdrom, cdb_, buffer, BUFFER_SIZE);
		switch (retval) {
			case 0:
				*phase = Status;
				break;
			case -1:
				break;
			default:
				*phase = DataIn;
				return retval;
		}
		return 0;
	}*/

	//TODO:    scsiDeviceDiskChanged(scsi);

	// check unit attention
	if (unitAttention && (mode & MODE_UNITATTENTION) &&
			(cdb_[0] != SCSIOP_INQUIRY) && (cdb_[0] != SCSIOP_REQUEST_SENSE)) {
		unitAttention = false;
		keycode = SENSE_POWER_ON;
		if (cdb_[0] == SCSIOP_TEST_UNIT_READY) {
			changed = false;
		}
		PRT_DEBUG("Unit Attention. This command is not executed.");
		return 0;
	}

	// check LUN
	if (((cdb_[1] & 0xe0) || lun) && (cdb_[0] != SCSIOP_REQUEST_SENSE) &&
			!(cdb_[0] == SCSIOP_INQUIRY && !(mode & MODE_NOVAXIS))) {
		keycode = SENSE_INVALID_LUN;
		PRT_DEBUG("check LUN error");
		return 0;
	}

	if (cdb_[0] != SCSIOP_REQUEST_SENSE) {
		keycode = SENSE_NO_SENSE;
	}

	int counter;

	if (cdb_[0] < SCSIOP_GROUP1) {
		currentSector = ((cdb_[1] & 0x1f) << 16) |
			(cdb_[2] << 8) | cdb_[3];
		currentLength = cdb_[4];

		switch (cdb_[0]) {
			case SCSIOP_TEST_UNIT_READY:
				PRT_DEBUG("TestUnitReady");
				testUnitReady();
				return 0;

			case SCSIOP_INQUIRY:
				PRT_DEBUG("Inquiry " << currentLength);
				counter = inquiry();
				if (counter) {
					*phase = DataIn;
				}
				return counter;

			case SCSIOP_REQUEST_SENSE:
				PRT_DEBUG("RequestSense");
				counter = requestSense();
				if (counter) {
					*phase = DataIn;
				}
				return counter;

			case SCSIOP_READ6:
				PRT_DEBUG("Read6: " << currentSector << " " << currentLength);
				if (currentLength == 0) {
					//currentLength = sectorSize >> 1;
					currentLength = 256;
				}
				if (checkAddress()) {
					counter = readSector(blocks);
					if(counter) {
						cdb[0] = SCSIOP_READ10;
						*phase = DataIn;
						return counter;
					}
				}
				return 0;

			case SCSIOP_WRITE6:
				PRT_DEBUG("Write6: " << currentSector << " " << currentLength);
				if (currentLength == 0) {
					//currentLength = sectorSize >> 1;
					currentLength = 256;
				}
				if (checkAddress() && !checkReadOnly()) {
					// TODO:                ledSetHd(1);
					if (currentLength >= BUFFER_BLOCK_SIZE) {
						*blocks = currentLength - BUFFER_BLOCK_SIZE;
						counter = BUFFER_SIZE;
					} else {
						counter = currentLength * sectorSize;
					}
					cdb[0] = SCSIOP_WRITE10;
					*phase = DataOut;
					return counter;
				}
				return 0;

			case SCSIOP_SEEK6:
				PRT_DEBUG("Seek6: " << currentSector);
				// TODO:            ledSetHd(1);
				currentLength = 1;
				checkAddress();
				return 0;

			case SCSIOP_MODE_SENSE:
				PRT_DEBUG("ModeSense: " << currentLength);
				counter = modeSense();
				if (counter) {
					*phase = DataIn;
				}
				return counter;

			case SCSIOP_FORMAT_UNIT:
				PRT_DEBUG("FormatUnit");
				formatUnit();
				return 0;

			case SCSIOP_START_STOP_UNIT:
				PRT_DEBUG("StartStopUnit");
				startStopUnit();
				return 0;

			case SCSIOP_REZERO_UNIT:
			case SCSIOP_REASSIGN_BLOCKS:
			case SCSIOP_RESERVE_UNIT:
			case SCSIOP_RELEASE_UNIT:
			case SCSIOP_SEND_DIAGNOSTIC:
				PRT_DEBUG("SCSI_Group0 dummy");
				return 0;
		}
	} else {
		currentSector = (cdb_[2] << 24) | (cdb_[3] << 16) |
			(cdb_[4] << 8)  |  cdb_[5];
		currentLength = (cdb_[7] << 8) + cdb_[8];

		switch (cdb_[0]) {
			case SCSIOP_READ10:
				PRT_DEBUG("Read10: " << currentSector << " " << currentLength);

				if (checkAddress()) {
					counter = readSector(blocks);
					if(counter) {
						*phase = DataIn;
						return counter;
					}
				}
				return 0;

			case SCSIOP_WRITE10:
				PRT_DEBUG("Write10: " << currentSector << " " << currentLength);

				if (checkAddress() && !checkReadOnly()) {
					if (currentLength >= BUFFER_BLOCK_SIZE) {
						*blocks = currentLength - BUFFER_BLOCK_SIZE;
						counter = BUFFER_SIZE;
					} else {
						counter = currentLength * sectorSize;
					}
					*phase = DataOut;
					return counter;
				}
				return 0;

			case SCSIOP_READ_CAPACITY:
				PRT_DEBUG("ReadCapacity");
				counter = readCapacity();
				if (counter) {
					*phase = DataIn;
				}
				return counter;

			case SCSIOP_SEEK10:
				PRT_DEBUG("Seek10: " << currentSector);
				//TODO            ledSetHd(1);
				currentLength = 1;
				checkAddress();
				return 0;
/*
			case SCSIOP_OPEN_MSX:
				PRT_DEBUG("openMSX");
				counter = openMSX();
				if (counter) {
					*phase = DataIn;
				}
				return counter;
*/
		}
	}

	PRT_DEBUG("unsupported command " << cdb_[0]);
	keycode = SENSE_INVALID_COMMAND_CODE;
	return 0;
}

int SCSIDevice::executingCmd(SCSI_PHASE* phase, int* blocks)
{
	int result = 0;

	/* TODO    if (archCdromIsXferComplete(cdrom, &result)) {
	 *phase = result ? DataIn : Status;
	 } else { */
	*phase = Execute;
	//    }
	*blocks = 0;
	return result;
}

byte SCSIDevice::msgIn()
{
	byte result = (byte)(message & 0xff);
	message = 0;
	//PRT_DEBUG("SCSIDevice " << scsiId << " msgIn returning " << result);
	return result;
}

/*
scsiDeviceMsgOut()
Notes:
    [out]
          -1: Busfree demand. (Please process it in the call origin.)
        bit2: Status phase demand. Error happend.
        bit1: Make it to a busfree if ATN has not been released.
        bit0: There is a message(MsgIn).
*/
int SCSIDevice::msgOut(byte value)
{
	PRT_DEBUG("SCSI #" << scsiId << " message out: " << (int)value);
	if (value & 0x80) {
		lun = value & 7;
		return 0;
	}

	switch (value) {
		case MSG_INITIATOR_DETECT_ERROR:
			keycode = SENSE_INITIATOR_DETECTED_ERR;
			return 6;

		case MSG_BUS_DEVICE_RESET:
			busReset();
		case MSG_ABORT:
			return -1;

		case MSG_REJECT:
		case MSG_PARITY_ERROR:
		case MSG_NO_OPERATION:
			return 2;
	}
	message = MSG_REJECT;
	return ((value >= 0x04) && (value <= 0x11)) ? 3 : 1;
}

unsigned SCSIDevice::getNbSectors() const
{
	return file->getSize() / sectorSize; 
}

// NOTE: UNUSED FOR NOW!
void SCSIDevice::readLogicalSector(unsigned sector, byte* buf)
{
	file->seek(512 * sector);
	file->read(buf, 512);
}

// NOTE: UNUSED FOR NOW!
void SCSIDevice::writeLogicalSector(unsigned sector, const byte* buf)
{
	file->seek(512 * sector);
	file->write(buf, 512);
}

SectorAccessibleDisk* SCSIDevice::getSectorAccessibleDisk()
{
	return this;
}

const std::string& SCSIDevice::getContainerName() const
{
	return name;
}

} // namespace openmsx
