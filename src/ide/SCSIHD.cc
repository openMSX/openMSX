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
 *
 *  NOTE: this version only supports a non-removable harddisk, as the class
 *  name suggests. Refer to revision 6526 of this file to see what was removed
 *  from the generic/parameterised code.
 */

#include "SCSIHD.hh"
#include "FileOperations.hh"
#include "FileException.hh"
#include "LedEvent.hh"
#include "MSXMotherBoard.hh"
#include "EventDistributor.hh"
#include "XMLElement.hh"
#include <algorithm>
#include <cstring>

//#include <iostream>
//#undef PRT_DEBUG
/*
#define  PRT_DEBUG(mes)                          \
	do {                                    \
	        std::cout << mes << std::endl;  \
	} while (0)
*/
using std::string;

namespace openmsx {

// Medium type (value like LS-120)
static const byte MT_UNKNOWN   = 0x00;
static const byte MT_2DD_UN    = 0x10;
static const byte MT_2DD       = 0x11;
static const byte MT_2HD_UN    = 0x20;
static const byte MT_2HD_12_98 = 0x22;
static const byte MT_2HD_12    = 0x23;
static const byte MT_2HD_144   = 0x24;
static const byte MT_LS120     = 0x31;
static const byte MT_NO_DISK   = 0x70;
static const byte MT_DOOR_OPEN = 0x71;
static const byte MT_FMT_ERROR = 0x72;

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
	'0', '1', '0', 'a'                         // product version (ASCII 4bytes)
};

static const unsigned SECTOR_SIZE = 512; // Always true for harddisk
static const unsigned BUFFER_BLOCK_SIZE = SCSIHD::BUFFER_SIZE / SECTOR_SIZE;

SCSIHD::SCSIHD(MSXMotherBoard& motherBoard_, const XMLElement& targetconfig,
		byte* const buf, unsigned mode_)
	  : HD(motherBoard_, targetconfig)
	  , motherBoard(motherBoard_)
	  , buffer(buf)
	  , mode(mode_)
	  , scsiId(targetconfig.getAttributeAsInt("id"))
{
	reset();
}

SCSIHD::~SCSIHD()
{
	PRT_DEBUG("hdd close for hdd " << int(scsiId));
}

void SCSIHD::reset()
{
	currentSector = 0;
	currentLength = 0;
	busReset();
}

void SCSIHD::busReset()
{
	PRT_DEBUG("SCSI: bus reset on " << int(scsiId));
	keycode = 0;
	unitAttention = (mode & MODE_UNITATTENTION);
}

void SCSIHD::disconnect()
{
	motherBoard.getEventDistributor().distributeEvent(
			new LedEvent(LedEvent::FDD, false, motherBoard));
}

// Check the initiator in the call origin.
bool SCSIHD::isSelected()
{
	lun = 0;
	return true;
}

unsigned SCSIHD::inquiry()
{
	unsigned length = currentLength;

	if (length == 0) return 0;

	memcpy(buffer + 2, inqdata + 2, 34);

	buffer[0] = SCSI::DT_DirectAccess;
	buffer[1] = 0; // removable

	if (!(mode & BIT_SCSI2)) {
		buffer[2] = 1;
		buffer[3] = 1;
		buffer[20] = '1';
	} else {
		if (mode & BIT_SCSI3) {
			buffer[2] = 5;
			buffer[20] = '3';
		}
	}

	if (mode & BIT_SCSI3) {
		length = std::min(length, 96u);
		buffer[4] = 91;
		if (length > 56) {
			memset(buffer + 56, 0, 40);
			buffer[58] = 0x03;
			buffer[60] = 0x01;
			buffer[61] = 0x80;
		}
	} else {
		length = std::min(length, 56u);
	}

	if (length > 36) {
		string filename = FileOperations::getFilename(getImageURL());
		filename.resize(20, ' ');
		memcpy(buffer + 36, filename.data(), 20);
	}
	return length;
}

unsigned SCSIHD::modeSense()
{
	byte* pBuffer = buffer;
	if ((currentLength > 0) && (cdb[2] == 3)) {
		unsigned total   = getNbSectors();
		byte media       = MT_UNKNOWN;
		byte sectors     = 64;
		byte blockLength = SECTOR_SIZE >> 8;
		byte tracks      = 8;
		byte size        = 4 + 24;
		byte removable   = 0x80; // == not removable

		memset(pBuffer + 2, 0, 34);

		if (total == 0) {
			media = MT_NO_DISK;
		}

		// Mode Parameter Header 4bytes
		pBuffer[1] = media; // Medium Type
		pBuffer[3] = 8;     // block descripter length
		pBuffer += 4;

		// Disable Block Descriptor check
		if (!(cdb[1] & 0x08)) {
			// Block Descriptor 8bytes
			pBuffer[1] = (total >> 16) & 0xff; // 1..3 Number of Blocks
			pBuffer[2] = (total >>  8) & 0xff;
			pBuffer[3] = (total >>  0) & 0xff;
			pBuffer[6] = blockLength & 0xff;   // 5..7 Block Length in Bytes
			pBuffer += 8;
			size   += 8;
		}

		// Format Device Page 24bytes
		pBuffer[ 0] = 3;           //     0 Page
		pBuffer[ 1] = 0x16;        //     1 Page Length
		pBuffer[ 3] = tracks;      //  2, 3 Tracks per Zone
		pBuffer[11] = sectors;     // 10,11 Sectors per Track
		pBuffer[12] = blockLength; // 12,13 Data Bytes per Physical Sector
		pBuffer[20] = removable;   // 20    bit7 Soft Sector bit5 Removable

		buffer[0] = size - 1;    // sense data length

		return std::min<unsigned>(currentLength, size);
	}
	keycode = SCSI::SENSE_INVALID_COMMAND_CODE;
	return 0;
}

unsigned SCSIHD::requestSense()
{
	unsigned length = currentLength;
	unsigned tmpKeycode = unitAttention ? SCSI::SENSE_POWER_ON : keycode;
	unitAttention = false;

	PRT_DEBUG("Request Sense: keycode = " << tmpKeycode);
	keycode = SCSI::SENSE_NO_SENSE;

	memset(buffer + 1, 0, 17);
	if (length == 0) {
		if (mode & BIT_SCSI2) {
			return 0;
		}
		buffer[ 0] = (tmpKeycode >>  8) & 0xff; // Sense code
		length = 4;
	} else {
		buffer[ 0] = 0x70;
		buffer[ 2] = (tmpKeycode >> 16) & 0xff; // Sense key
		buffer[ 7] = 10;                        // Additional sense length
		buffer[12] = (tmpKeycode >>  8) & 0xff; // Additional sense code
		buffer[13] = (tmpKeycode >>  0) & 0xff; // Additional sense code qualifier
		length = std::min(length, 18u);
	}
	return length;
}

bool SCSIHD::checkReadOnly()
{
	if (isImageReadOnly()) {
		keycode = SCSI::SENSE_WRITE_PROTECT;
		return true;
	}
	return false;
}

unsigned SCSIHD::readCapacity()
{
	unsigned block = getNbSectors();

	if (block == 0) {
		keycode = SCSI::SENSE_MEDIUM_NOT_PRESENT;
		PRT_DEBUG("hdd " << int(scsiId) << ": drive not ready");
		return 0;
	}

	PRT_DEBUG("total block: " << block);

	--block;
	buffer[0] = (block >> 24) & 0xff;
	buffer[1] = (block >> 16) & 0xff;
	buffer[2] = (block >>  8) & 0xff;
	buffer[3] = (block >>  0) & 0xff;
	buffer[4] = 0;
	buffer[5] = 0;
	buffer[6] = (SECTOR_SIZE >> 8) & 0xff;
	buffer[7] = 0;

	return 8;
}

bool SCSIHD::checkAddress()
{
	unsigned total = getNbSectors();
	if (total == 0) {
		keycode = SCSI::SENSE_MEDIUM_NOT_PRESENT;
		PRT_DEBUG("hdd " << int(scsiId) << ": drive not ready");
		return false;
	}

	if ((currentLength > 0) && (currentSector + currentLength <= total)) {
		return true;
	}
	PRT_DEBUG("hdd " << int(scsiId) << ": IllegalBlockAddress");
	keycode = SCSI::SENSE_ILLEGAL_BLOCK_ADDRESS;
	return false;
}

// Execute scsiDeviceCheckAddress previously.
unsigned SCSIHD::readSector(unsigned& blocks)
{
	motherBoard.getEventDistributor().distributeEvent(
		new LedEvent(LedEvent::FDD, true, motherBoard));

	unsigned numSectors = std::min(currentLength, BUFFER_BLOCK_SIZE);
	unsigned counter = currentLength * SECTOR_SIZE;

	PRT_DEBUG("hdd#" << int(scsiId) << " read sector: " << currentSector << " " << numSectors);
	try {
		readFromImage(SECTOR_SIZE * currentSector,
		              SECTOR_SIZE * numSectors,
			      buffer);
		currentSector += numSectors;
		currentLength -= numSectors;
		blocks = currentLength;
		return counter;
	} catch (FileException& e) {
		blocks = 0;
		keycode = SCSI::SENSE_UNRECOVERED_READ_ERROR;
		return 0;
	}
}

unsigned SCSIHD::dataIn(unsigned& blocks)
{
	if (cdb[0] == SCSI::OP_READ10) {
		unsigned counter = readSector(blocks);
		if (counter) {
			return counter;
		}
	}
	PRT_DEBUG("dataIn error " << cdb[0]);
	blocks = 0;
	return 0;
}

// Execute scsiDeviceCheckAddress and scsiDeviceCheckReadOnly previously.
unsigned SCSIHD::writeSector(unsigned& blocks)
{
	motherBoard.getEventDistributor().distributeEvent(
		new LedEvent(LedEvent::FDD, true, motherBoard));

	unsigned numSectors = std::min(currentLength, BUFFER_BLOCK_SIZE);

	PRT_DEBUG("hdd#" << int(scsiId) << " write sector: " << currentSector << " " << numSectors);
	try {
		writeToImage(SECTOR_SIZE * currentSector,
		             SECTOR_SIZE * numSectors,
			     buffer);
		currentSector += numSectors;
		currentLength -= numSectors;

		unsigned tmp = std::min(currentLength, BUFFER_BLOCK_SIZE);
		blocks = currentLength - tmp;
		unsigned counter = tmp * SECTOR_SIZE;
		return counter;
	} catch (FileException& e) {
		keycode = SCSI::SENSE_WRITE_FAULT;
		blocks = 0;
		return 0;
	}
}

unsigned SCSIHD::dataOut(unsigned& blocks)
{
	if (cdb[0] == SCSI::OP_WRITE10) {
		return writeSector(blocks);
	}
	PRT_DEBUG("dataOut error " << int(cdb[0]));
	blocks = 0;
	return 0;
}

//  MBR erase only
void SCSIHD::formatUnit()
{
	if (!checkReadOnly()) {
		memset(buffer, 0, SECTOR_SIZE);
		try {
			writeLogicalSector(0, buffer);
			unitAttention = true;
		} catch (FileException& e) {
			keycode = SCSI::SENSE_WRITE_FAULT;
		}
	}
}

byte SCSIHD::getStatusCode()
{
	byte result = keycode ? SCSI::ST_CHECK_CONDITION : SCSI::ST_GOOD;
	PRT_DEBUG("SCSI status code: \n" << int(result));
	return result;
}

unsigned SCSIHD::executeCmd(const byte* cdb_, SCSI::Phase& phase, unsigned& blocks)
{
	PRT_DEBUG("SCSI Command: " << int(cdb[0]));
	memcpy(cdb, cdb_, 12);
	message = 0;
	phase = SCSI::STATUS;
	blocks = 0;

	// check unit attention
	if (unitAttention && (mode & MODE_UNITATTENTION) &&
	    (cdb[0] != SCSI::OP_INQUIRY) && (cdb[0] != SCSI::OP_REQUEST_SENSE)) {
		unitAttention = false;
		keycode = SCSI::SENSE_POWER_ON;
		if (cdb[0] == SCSI::OP_TEST_UNIT_READY) {
			// changed = false;
		}
		PRT_DEBUG("Unit Attention. This command is not executed.");
		return 0;
	}

	// check LUN
	if (((cdb[1] & 0xe0) || lun) && (cdb[0] != SCSI::OP_REQUEST_SENSE) &&
	    !(cdb[0] == SCSI::OP_INQUIRY && !(mode & MODE_NOVAXIS))) {
		keycode = SCSI::SENSE_INVALID_LUN;
		PRT_DEBUG("check LUN error");
		return 0;
	}

	if (cdb[0] != SCSI::OP_REQUEST_SENSE) {
		keycode = SCSI::SENSE_NO_SENSE;
	}

	if (cdb[0] < SCSI::OP_GROUP1) {
		currentSector = ((cdb[1] & 0x1f) << 16) | (cdb[2] << 8) | cdb[3];
		currentLength = cdb[4];

		switch (cdb[0]) {
		case SCSI::OP_TEST_UNIT_READY:
			PRT_DEBUG("TestUnitReady");
			return 0;

		case SCSI::OP_INQUIRY: {
			PRT_DEBUG("Inquiry " << currentLength);
			unsigned counter = inquiry();
			if (counter) {
				phase = SCSI::DATA_IN;
			}
			return counter;
		}
		case SCSI::OP_REQUEST_SENSE: {
			PRT_DEBUG("RequestSense");
			unsigned counter = requestSense();
			if (counter) {
				phase = SCSI::DATA_IN;
			}
			return counter;
		}
		case SCSI::OP_READ6:
			PRT_DEBUG("Read6: " << currentSector << " " << currentLength);
			if (currentLength == 0) {
				currentLength = SECTOR_SIZE / 2;
			}
			if (checkAddress()) {
				unsigned counter = readSector(blocks);
				if (counter) {
					cdb[0] = SCSI::OP_READ10;
					phase = SCSI::DATA_IN;
					return counter;
				}
			}
			return 0;

		case SCSI::OP_WRITE6:
			PRT_DEBUG("Write6: " << currentSector << " " << currentLength);
			if (currentLength == 0) {
				currentLength = SECTOR_SIZE / 2;
			}
			if (checkAddress() && !checkReadOnly()) {
				motherBoard.getEventDistributor().distributeEvent(
					new LedEvent(LedEvent::FDD, true,
						motherBoard));
				unsigned tmp = std::min(currentLength, BUFFER_BLOCK_SIZE);
				blocks = currentLength - tmp;
				unsigned counter = tmp * SECTOR_SIZE;
				cdb[0] = SCSI::OP_WRITE10;
				phase = SCSI::DATA_OUT;
				return counter;
			}
			return 0;

		case SCSI::OP_SEEK6:
			PRT_DEBUG("Seek6: " << currentSector);
			motherBoard.getEventDistributor().distributeEvent(
				new LedEvent(LedEvent::FDD, true, motherBoard));
			currentLength = 1;
			checkAddress();
			return 0;

		case SCSI::OP_MODE_SENSE: {
			PRT_DEBUG("ModeSense: " << currentLength);
			unsigned counter = modeSense();
			if (counter) {
				phase = SCSI::DATA_IN;
			}
			return counter;
		}
		case SCSI::OP_FORMAT_UNIT:
			PRT_DEBUG("FormatUnit");
			formatUnit();
			return 0;

		case SCSI::OP_START_STOP_UNIT:
			PRT_DEBUG("StartStopUnit (Not supported for this device.)");
			return 0;

		case SCSI::OP_REZERO_UNIT:
		case SCSI::OP_REASSIGN_BLOCKS:
		case SCSI::OP_RESERVE_UNIT:
		case SCSI::OP_RELEASE_UNIT:
		case SCSI::OP_SEND_DIAGNOSTIC:
			PRT_DEBUG("SCSI_Group0 dummy");
			return 0;
		}
	} else {
		currentSector = (cdb[2] << 24) | (cdb[3] << 16) |
		                (cdb[4] <<  8) | (cdb[5] <<  0);
		currentLength = (cdb[7] <<  8) | (cdb[8] <<  0);

		switch (cdb[0]) {
		case SCSI::OP_READ10:
			PRT_DEBUG("Read10: " << currentSector << " " << currentLength);

			if (checkAddress()) {
				unsigned counter = readSector(blocks);
				if (counter) {
					phase = SCSI::DATA_IN;
					return counter;
				}
			}
			return 0;

		case SCSI::OP_WRITE10:
			PRT_DEBUG("Write10: " << currentSector << " " << currentLength);

			if (checkAddress() && !checkReadOnly()) {
				unsigned tmp = std::min(currentLength, BUFFER_BLOCK_SIZE);
				blocks = currentLength - tmp;
				unsigned counter = tmp * SECTOR_SIZE;
				phase = SCSI::DATA_OUT;
				return counter;
			}
			return 0;

		case SCSI::OP_READ_CAPACITY: {
			PRT_DEBUG("ReadCapacity");
			unsigned counter = readCapacity();
			if (counter) {
				phase = SCSI::DATA_IN;
			}
			return counter;
		}
		case SCSI::OP_SEEK10:
			PRT_DEBUG("Seek10: " << currentSector);
			motherBoard.getEventDistributor().distributeEvent(
				new LedEvent(LedEvent::FDD, true, motherBoard));
			currentLength = 1;
			checkAddress();
			return 0;
		}
	}

	PRT_DEBUG("unsupported command " << cdb[0]);
	keycode = SCSI::SENSE_INVALID_COMMAND_CODE;
	return 0;
}

unsigned SCSIHD::executingCmd(SCSI::Phase& phase, unsigned& blocks)
{
	phase = SCSI::EXECUTE;
	blocks = 0;
	return 0; // Always for non-CD-ROM it seems
}

byte SCSIHD::msgIn()
{
	byte result = message;
	message = 0;
	//PRT_DEBUG("SCSIDevice " << int(scsiId) << " msgIn returning " << result);
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
int SCSIHD::msgOut(byte value)
{
	PRT_DEBUG("SCSI #" << int(scsiId) << " message out: " << int(value));
	if (value & 0x80) {
		lun = value & 7;
		return 0;
	}

	switch (value) {
	case SCSI::MSG_INITIATOR_DETECT_ERROR:
		keycode = SCSI::SENSE_INITIATOR_DETECTED_ERR;
		return 6;

	case SCSI::MSG_BUS_DEVICE_RESET:
		busReset();
		// fall-through
	case SCSI::MSG_ABORT:
		return -1;

	case SCSI::MSG_REJECT:
	case SCSI::MSG_PARITY_ERROR:
	case SCSI::MSG_NO_OPERATION:
		return 2;
	}
	message = SCSI::MSG_REJECT;
	return ((value >= 0x04) && (value <= 0x11)) ? 3 : 1;
}

unsigned SCSIHD::getNbSectors() const
{
	return getImageSize() / SECTOR_SIZE;
}

// NOTE: UNUSED FOR NOW!
void SCSIHD::readLogicalSector(unsigned sector, byte* buf)
{
	readFromImage(SECTOR_SIZE * sector, SECTOR_SIZE, buf);
}

// NOTE: UNUSED FOR NOW!
void SCSIHD::writeLogicalSector(unsigned sector, const byte* buf)
{
	writeToImage(SECTOR_SIZE * sector, SECTOR_SIZE, buf);
}

SectorAccessibleDisk* SCSIHD::getSectorAccessibleDisk()
{
	return this;
}

const std::string& SCSIHD::getContainerName() const
{
	return getName();
}

} // namespace openmsx
