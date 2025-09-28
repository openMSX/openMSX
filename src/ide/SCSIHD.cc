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
 *  NOTE: this version only supports a non-removable hard disk, as the class
 *  name suggests. Refer to revision 6526 of this file to see what was removed
 *  from the generic/parameterized code.
 */

#include "SCSIHD.hh"

#include "DeviceConfig.hh"
#include "FileOperations.hh"
#include "LedStatus.hh"
#include "MSXException.hh"
#include "MSXMotherBoard.hh"
#include "serialize.hh"

#include "endian.hh"
#include "narrow.hh"
#include "one_of.hh"
#include "xrange.hh"

#include <algorithm>
#include <cstring>

namespace openmsx {

// Medium type (value like LS-120)
static constexpr uint8_t MT_UNKNOWN   = 0x00;
static constexpr uint8_t MT_2DD_UN    = 0x10;
static constexpr uint8_t MT_2DD       = 0x11;
static constexpr uint8_t MT_2HD_UN    = 0x20;
static constexpr uint8_t MT_2HD_12_98 = 0x22;
static constexpr uint8_t MT_2HD_12    = 0x23;
static constexpr uint8_t MT_2HD_144   = 0x24;
static constexpr uint8_t MT_LS120     = 0x31;
static constexpr uint8_t MT_NO_DISK   = 0x70;
static constexpr uint8_t MT_DOOR_OPEN = 0x71;
static constexpr uint8_t MT_FMT_ERROR = 0x72;

static constexpr std::array<uint8_t, 36> inqData = {
	  0,   // bit5-0 device type code.
	  0,   // bit7 = 1 removable device
	  2,   // bit7,6 ISO version. bit5,4,3 ECMA version.
	       // bit2,1,0 ANSI Version (001=SCSI1, 010=SCSI2)
	  2,   // bit7 AENC. bit6 TrmIOP.
	       // bit3-0 Response Data Format. (0000=SCSI1, 0001=CCS, 0010=SCSI2)
	 51,   // additional length
	  0, 0,// reserved
	  0,   // bit7 RelAdr, bit6 WBus32, bit5 Wbus16, bit4 Sync, bit3 Linked,
	       // bit2 reserved bit1 CmdQue, bit0 SftRe
	'o', 'p', 'e', 'n', 'M', 'S', 'X', ' ',    // vendor ID (8bytes)
	'S', 'C', 'S', 'I', '2', ' ', 'H', 'a',    // product ID (16bytes)
	'r', 'd', 'd', 'i', 's', 'k', ' ', ' ',
	'0', '1', '0', 'a'                         // product version (ASCII 4bytes)
};

static constexpr unsigned BUFFER_BLOCK_SIZE = SCSIHD::BUFFER_SIZE /
                                              SectorAccessibleDisk::SECTOR_SIZE;

SCSIHD::SCSIHD(const DeviceConfig& targetConfig,
               AlignedBuffer& buf, unsigned mode_)
	: HD(targetConfig)
	, buffer(buf)
	, mode(mode_)
	, scsiId(narrow_cast<uint8_t>(targetConfig.getAttributeValueAsInt("id", 0)))
{
	reset();
}

void SCSIHD::reset()
{
	currentSector = 0;
	currentLength = 0;
	busReset();
}

void SCSIHD::busReset()
{
	keycode = 0;
	unitAttention = (mode & MODE_UNITATTENTION) != 0;
}

void SCSIHD::disconnect()
{
	getMotherBoard().getLedStatus().setLed(LedStatus::FDD, false);
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

	buffer[0] = SCSI::DT_DirectAccess;
	buffer[1] = 0; // removable
	std::ranges::copy(subspan(inqData, 2), &buffer[2]);

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
		std::string imageName(FileOperations::getFilename(
		                       getImageName().getOriginal()));
		imageName.resize(20, ' ');
		std::ranges::copy(imageName, &buffer[36]);
	}
	return length;
}

unsigned SCSIHD::modeSense()
{
	uint8_t* pBuffer = buffer;
	if ((currentLength > 0) && (cdb[2] == 3)) {
		// TODO check for too many sectors
		auto total       = unsigned(getNbSectors());
		uint8_t media       = MT_UNKNOWN;
		uint8_t sectors     = 64;
		uint8_t blockLength = SECTOR_SIZE >> 8;
		uint8_t tracks      = 8;
		uint8_t size        = 4 + 24;
		uint8_t removable   = 0x80; // == not removable

		memset(pBuffer + 2, 0, 34);

		if (total == 0) {
			media = MT_NO_DISK;
		}

		// Mode Parameter Header 4bytes
		pBuffer[1] = media; // Medium Type
		pBuffer[3] = 8;     // block descriptor length
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
	if (isWriteProtected()) {
		keycode = SCSI::SENSE_WRITE_PROTECT;
		return true;
	}
	return false;
}

unsigned SCSIHD::readCapacity()
{
	// TODO check for overflow
	auto block = unsigned(getNbSectors());

	if (block == 0) {
		// drive not ready
		keycode = SCSI::SENSE_MEDIUM_NOT_PRESENT;
		return 0;
	}

	--block;
	Endian::writeB32(&buffer[0], block);
	Endian::writeB32(&buffer[4], SECTOR_SIZE); // TODO is this a 32 bit field or 2x16-bit fields where the first field happens to have the value 0?
	return 8;
}

bool SCSIHD::checkAddress()
{
	auto total = unsigned(getNbSectors());
	if (total == 0) {
		// drive not ready
		keycode = SCSI::SENSE_MEDIUM_NOT_PRESENT;
		return false;
	}

	if ((currentLength > 0) && (currentSector + currentLength <= total)) {
		return true;
	}
	keycode = SCSI::SENSE_ILLEGAL_BLOCK_ADDRESS;
	return false;
}

// Execute scsiDeviceCheckAddress previously.
unsigned SCSIHD::readSectors(unsigned& blocks)
{
	getMotherBoard().getLedStatus().setLed(LedStatus::FDD, true);

	unsigned numSectors = std::min(currentLength, BUFFER_BLOCK_SIZE);
	unsigned counter = currentLength * SECTOR_SIZE;

	try {
		for (auto i : xrange(numSectors)) {
			auto* sbuf = aligned_cast<SectorBuffer*>(buffer);
			readSector(currentSector, sbuf[i]);
			++currentSector;
			--currentLength;
		}
		blocks = currentLength;
		return counter;
	} catch (MSXException&) {
		blocks = 0;
		keycode = SCSI::SENSE_UNRECOVERED_READ_ERROR;
		return 0;
	}
}

unsigned SCSIHD::dataIn(unsigned& blocks)
{
	if (cdb[0] == SCSI::OP_READ10) {
		unsigned counter = readSectors(blocks);
		if (counter) return counter;
	}
	// error
	blocks = 0;
	return 0;
}

// Execute scsiDeviceCheckAddress and scsiDeviceCheckReadOnly previously.
unsigned SCSIHD::writeSectors(unsigned& blocks)
{
	getMotherBoard().getLedStatus().setLed(LedStatus::FDD, true);

	unsigned numSectors = std::min(currentLength, BUFFER_BLOCK_SIZE);

	try {
		for (auto i : xrange(numSectors)) {
			const auto* sbuf = aligned_cast<const SectorBuffer*>(buffer);
			writeSector(currentSector, sbuf[i]);
			++currentSector;
			--currentLength;
		}

		unsigned tmp = std::min(currentLength, BUFFER_BLOCK_SIZE);
		blocks = currentLength - tmp;
		unsigned counter = tmp * SECTOR_SIZE;
		return counter;
	} catch (MSXException&) {
		keycode = SCSI::SENSE_WRITE_FAULT;
		blocks = 0;
		return 0;
	}
}

unsigned SCSIHD::dataOut(unsigned& blocks)
{
	if (cdb[0] == SCSI::OP_WRITE10) {
		return writeSectors(blocks);
	}
	// error
	blocks = 0;
	return 0;
}

//  MBR erase only
void SCSIHD::formatUnit()
{
	if (!checkReadOnly()) {
		auto& sbuf = *aligned_cast<SectorBuffer*>(buffer);
		memset(&sbuf, 0, sizeof(sbuf));
		try {
			writeSector(0, sbuf);
			unitAttention = true;
		} catch (MSXException&) {
			keycode = SCSI::SENSE_WRITE_FAULT;
		}
	}
}

uint8_t SCSIHD::getStatusCode()
{
	return keycode ? SCSI::ST_CHECK_CONDITION : SCSI::ST_GOOD;
}

unsigned SCSIHD::executeCmd(std::span<const uint8_t, 12> cdb_, SCSI::Phase& phase, unsigned& blocks)
{
	using enum SCSI::Phase;

	copy_to_range(cdb_, cdb);
	message = 0;
	phase = STATUS;
	blocks = 0;

	// check unit attention
	if (unitAttention && (mode & MODE_UNITATTENTION) &&
	    (cdb[0] != one_of(SCSI::OP_INQUIRY, SCSI::OP_REQUEST_SENSE))) {
		unitAttention = false;
		keycode = SCSI::SENSE_POWER_ON;
		if (cdb[0] == SCSI::OP_TEST_UNIT_READY) {
			// changed = false;
		}
		// Unit Attention. This command is not executed.
		return 0;
	}

	// check LUN
	if (((cdb[1] & 0xe0) || lun) && (cdb[0] != SCSI::OP_REQUEST_SENSE) &&
	    !(cdb[0] == SCSI::OP_INQUIRY && !(mode & MODE_NOVAXIS))) {
		keycode = SCSI::SENSE_INVALID_LUN;
		// check LUN error
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
			return 0;

		case SCSI::OP_INQUIRY: {
			unsigned counter = inquiry();
			if (counter) {
				phase = DATA_IN;
			}
			return counter;
		}
		case SCSI::OP_REQUEST_SENSE: {
			unsigned counter = requestSense();
			if (counter) {
				phase = DATA_IN;
			}
			return counter;
		}
		case SCSI::OP_READ6:
			if (currentLength == 0) {
				currentLength = SECTOR_SIZE / 2;
			}
			if (checkAddress()) {
				unsigned counter = readSectors(blocks);
				if (counter) {
					cdb[0] = SCSI::OP_READ10;
					phase = DATA_IN;
					return counter;
				}
			}
			return 0;

		case SCSI::OP_WRITE6:
			if (currentLength == 0) {
				currentLength = SECTOR_SIZE / 2;
			}
			if (checkAddress() && !checkReadOnly()) {
				getMotherBoard().getLedStatus().setLed(LedStatus::FDD, true);
				unsigned tmp = std::min(currentLength, BUFFER_BLOCK_SIZE);
				blocks = currentLength - tmp;
				unsigned counter = tmp * SECTOR_SIZE;
				cdb[0] = SCSI::OP_WRITE10;
				phase = DATA_OUT;
				return counter;
			}
			return 0;

		case SCSI::OP_SEEK6:
			getMotherBoard().getLedStatus().setLed(LedStatus::FDD, true);
			currentLength = 1;
			(void)checkAddress();
			return 0;

		case SCSI::OP_MODE_SENSE: {
			unsigned counter = modeSense();
			if (counter) {
				phase = DATA_IN;
			}
			return counter;
		}
		case SCSI::OP_FORMAT_UNIT:
			formatUnit();
			return 0;

		case SCSI::OP_START_STOP_UNIT:
			// Not supported for this device
			return 0;

		case SCSI::OP_REZERO_UNIT:
		case SCSI::OP_REASSIGN_BLOCKS:
		case SCSI::OP_RESERVE_UNIT:
		case SCSI::OP_RELEASE_UNIT:
		case SCSI::OP_SEND_DIAGNOSTIC:
			// SCSI_Group0 dummy
			return 0;
		}
	} else {
		currentSector = Endian::read_UA_B32(&cdb[2]);
		currentLength = Endian::read_UA_B16(&cdb[7]);

		switch (cdb[0]) {
		case SCSI::OP_READ10:
			if (checkAddress()) {
				unsigned counter = readSectors(blocks);
				if (counter) {
					phase = DATA_IN;
					return counter;
				}
			}
			return 0;

		case SCSI::OP_WRITE10:
			if (checkAddress() && !checkReadOnly()) {
				unsigned tmp = std::min(currentLength, BUFFER_BLOCK_SIZE);
				blocks = currentLength - tmp;
				unsigned counter = tmp * SECTOR_SIZE;
				phase = DATA_OUT;
				return counter;
			}
			return 0;

		case SCSI::OP_READ_CAPACITY: {
			unsigned counter = readCapacity();
			if (counter) {
				phase = DATA_IN;
			}
			return counter;
		}
		case SCSI::OP_SEEK10:
			getMotherBoard().getLedStatus().setLed(LedStatus::FDD, true);
			currentLength = 1;
			(void)checkAddress();
			return 0;
		}
	}

	// unsupported command
	keycode = SCSI::SENSE_INVALID_COMMAND_CODE;
	return 0;
}

unsigned SCSIHD::executingCmd(SCSI::Phase& phase, unsigned& blocks)
{
	phase = SCSI::Phase::EXECUTE;
	blocks = 0;
	return 0; // Always for non-CD-ROM it seems
}

uint8_t SCSIHD::msgIn()
{
	uint8_t result = message;
	message = 0;
	return result;
}

/*
scsiDeviceMsgOut()
Notes:
    [out]
	  -1: Busfree demand. (Please process it in the call origin.)
	bit2: Status phase demand. Error happened.
	bit1: Make it to a busfree if ATN has not been released.
	bit0: There is a message(MsgIn).
*/
int SCSIHD::msgOut(uint8_t value)
{
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
		[[fallthrough]];
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


template<typename Archive>
void SCSIHD::serialize(Archive& ar, unsigned /*version*/)
{
	// don't serialize SCSIDevice, SectorAccessibleDisk, DiskContainer
	// base classes
	ar.template serializeBase<HD>(*this);
	ar.serialize("keycode",       keycode,
	             "currentSector", currentSector,
	             "currentLength", currentLength,
	             "unitAttention", unitAttention,
	             "message",       message,
	             "lun",           lun);
	ar.serialize_blob("cdb", cdb);
}
INSTANTIATE_SERIALIZE_METHODS(SCSIHD);
REGISTER_POLYMORPHIC_INITIALIZER(SCSIDevice, SCSIHD, "SCSIHD");

} // namespace openmsx
