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
 *  NOTE: this version supports a removable LS-120 disk, as the class
 *  name suggests. Refer to revision 6526 of SCSIHD.cc to see what was removed
 *  from the generic/parameterised code.
 */

#include "SCSILS120.hh"
#include "FileOperations.hh"
#include "FileException.hh"
#include "FilePool.hh"
#include "LedStatus.hh"
#include "MSXMotherBoard.hh"
#include "Reactor.hh"
#include "DeviceConfig.hh"
#include "RecordedCommand.hh"
#include "CliComm.hh"
#include "TclObject.hh"
#include "CommandException.hh"
#include "FileContext.hh"
#include "endian.hh"
#include "serialize.hh"
#include "memory.hh"
#include <algorithm>
#include <vector>
#include <cstring>

using std::string;
using std::vector;

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
	'S', 'C', 'S', 'I', '2', ' ', 'L', 'S',    // product ID (16bytes)
	'-', '1', '2', '0', 'd', 'i', 's', 'k',
	'0', '1', '0', 'a'                         // product version (ASCII 4bytes)
};

// for FDSFORM.COM
static const char fds120[28 + 1]  = "IODATA  LS-120 COSM     0001";

static const unsigned BUFFER_BLOCK_SIZE = SCSIDevice::BUFFER_SIZE /
                                          SectorAccessibleDisk::SECTOR_SIZE;

class LSXCommand final : public RecordedCommand
{
public:
	LSXCommand(CommandController& commandController,
	           StateChangeDistributor& stateChangeDistributor,
	           Scheduler& scheduler, SCSILS120& ls);
	void execute(array_ref<TclObject> tokens,
	             TclObject& result, EmuTime::param time) override;
	string help(const vector<string>& tokens) const override;
	void tabCompletion(vector<string>& tokens) const override;
private:
	SCSILS120& ls;
};

SCSILS120::SCSILS120(const DeviceConfig& targetconfig,
                     AlignedBuffer& buf, unsigned mode_)
	: motherBoard(targetconfig.getMotherBoard())
	, buffer(buf)
	, name("lsX")
	, mode(mode_)
	, scsiId(targetconfig.getAttributeAsInt("id"))
{
	lsInUse = motherBoard.getSharedStuff<LSInUse>("lsInUse");

	unsigned id = 0;
	while ((*lsInUse)[id]) {
		++id;
		if (id == MAX_LS) {
			throw MSXException("Too many LSs");
		}
	}
	name[2] = char('a' + id);
	(*lsInUse)[id] = true;
	lsxCommand = make_unique<LSXCommand>(
		motherBoard.getCommandController(),
		motherBoard.getStateChangeDistributor(),
		motherBoard.getScheduler(), *this);

	reset();
}

SCSILS120::~SCSILS120()
{
	motherBoard.getMSXCliComm().update(CliComm::HARDWARE, name, "remove");

	unsigned id = name[2] - 'a';
	assert((*lsInUse)[id]);
	(*lsInUse)[id] = false;
}

void SCSILS120::reset()
{
	mediaChanged  = false;
	currentSector = 0;
	currentLength = 0;
	busReset();
}

void SCSILS120::busReset()
{
	keycode = 0;
	unitAttention = (mode & MODE_UNITATTENTION) != 0;
}

void SCSILS120::disconnect()
{
	motherBoard.getLedStatus().setLed(LedStatus::FDD, false);
}

// Check the initiator in the call origin.
bool SCSILS120::isSelected()
{
	lun = 0;
	return file.is_open();
}

bool SCSILS120::getReady()
{
	if (file.is_open()) return true;
	keycode = SCSI::SENSE_MEDIUM_NOT_PRESENT;
	return false;
}

void SCSILS120::testUnitReady()
{
	if ((mode & MODE_NOVAXIS) == 0) {
		if (getReady() && mediaChanged && (mode & MODE_MEGASCSI)) {
			// Disk change is surely sent for the driver of MEGA-SCSI.
			keycode = SCSI::SENSE_POWER_ON;
		}
	}
	mediaChanged = false;
}

void SCSILS120::startStopUnit()
{
	switch (cdb[4]) {
	case 2: // Eject
		eject();
		break;
	case 3: // Insert  TODO: how can this happen?
		//if (!diskPresent(diskId)) {
		//	*disk = disk;
		//	updateExtendedDiskName(diskId, disk->fileName, disk->fileNameInZip);
		//	boardChangeDiskette(diskId, disk->fileName, disk->fileNameInZip);
		//}
		break;
	}
}

unsigned SCSILS120::inquiry()
{
	auto total      = getNbSectors();
	unsigned length = currentLength;

	bool fdsmode = (total > 0) && (total <= 2880);

	if (length == 0) return 0;

	if (fdsmode) {
		memcpy(buffer + 2, inqdata + 2, 6);
		memcpy(buffer + 8, fds120, 28);
	} else {
		memcpy(buffer + 2, inqdata + 2, 34);
	}

	buffer[0] = SCSI::DT_DirectAccess;
	buffer[1] = 0x80; // removable

	if (!(mode & BIT_SCSI2)) {
		buffer[2] = 1;
		buffer[3] = 1;
		if (!fdsmode) buffer[20] = '1';
	} else {
		if (mode & BIT_SCSI3) {
			buffer[2] = 5;
			if (!fdsmode) buffer[20] = '3';
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
		string filename = FileOperations::getFilename(file.getURL()).str();
		filename.resize(20, ' ');
		memcpy(buffer + 36, filename.data(), 20);
	}
	return length;
}

unsigned SCSILS120::modeSense()
{
	byte* pBuffer = buffer;

	if ((currentLength > 0) && (cdb[2] == 3)) {
		auto total       = getNbSectors();
		byte media       = MT_UNKNOWN;
		byte sectors     = 64;
		byte blockLength = SECTOR_SIZE >> 8;
		byte tracks      = 8;
		byte size        = 4 + 24;
		byte removable   = 0xa0;

		memset(pBuffer + 2, 0, 34);

		if (total == 0) {
			media = MT_NO_DISK;
		} else {
			if (total == 1440) {
				media       = MT_2DD;
				sectors     = 9;
				blockLength = 2048 >> 8; // FDS-120 value
				tracks      = 160;
			} else {
				if (total == 2880) {
					media       = MT_2HD_144;
					sectors     = 18;
					blockLength = 2048 >> 8;
					tracks      = 160;
				}
			}
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

unsigned SCSILS120::requestSense()
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

bool SCSILS120::checkReadOnly()
{
	if (file.isReadOnly()) {
		keycode = SCSI::SENSE_WRITE_PROTECT;
		return true;
	}
	return false;
}

unsigned SCSILS120::readCapacity()
{
	unsigned block = unsigned(getNbSectors());

	if (block == 0) {
		// drive not ready
		keycode = SCSI::SENSE_MEDIUM_NOT_PRESENT;
		return 0;
	}

	--block;
	Endian::writeB32(&buffer[0], block);
	Endian::writeB32(&buffer[4], SECTOR_SIZE); // TODO see SCSIHD

	return 8;
}

bool SCSILS120::checkAddress()
{
	unsigned total = unsigned(getNbSectors());
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
unsigned SCSILS120::readSector(unsigned& blocks)
{
	motherBoard.getLedStatus().setLed(LedStatus::FDD, true);

	unsigned numSectors = std::min(currentLength, BUFFER_BLOCK_SIZE);
	unsigned counter = currentLength * SECTOR_SIZE;

	try {
		// TODO: somehow map this to SectorAccessibleDisk::readSector?
		file.seek(SECTOR_SIZE * currentSector);
		file.read(buffer, SECTOR_SIZE * numSectors);
		currentSector += numSectors;
		currentLength -= numSectors;
		blocks = currentLength;
		return counter;
	} catch (FileException&) {
		blocks = 0;
		keycode = SCSI::SENSE_UNRECOVERED_READ_ERROR;
		return 0;
	}
}

unsigned SCSILS120::dataIn(unsigned& blocks)
{
	if (cdb[0] == SCSI::OP_READ10) {
		unsigned counter = readSector(blocks);
		if (counter) {
			return counter;
		}
	}
	// error
	blocks = 0;
	return 0;
}

// Execute scsiDeviceCheckAddress and scsiDeviceCheckReadOnly previously.
unsigned SCSILS120::writeSector(unsigned& blocks)
{
	motherBoard.getLedStatus().setLed(LedStatus::FDD, true);

	unsigned numSectors = std::min(currentLength, BUFFER_BLOCK_SIZE);

	// TODO: somehow map this to SectorAccessibleDisk::writeSector?
	try {
		file.seek(SECTOR_SIZE * currentSector);
		file.write(buffer, SECTOR_SIZE * numSectors);
		currentSector += numSectors;
		currentLength -= numSectors;

		unsigned tmp = std::min(currentLength, BUFFER_BLOCK_SIZE);
		blocks = currentLength - tmp;
		unsigned counter = tmp * SECTOR_SIZE;
		return counter;
	} catch (FileException&) {
		keycode = SCSI::SENSE_WRITE_FAULT;
		blocks = 0;
		return 0;
	}
}

unsigned SCSILS120::dataOut(unsigned& blocks)
{
	if (cdb[0] == SCSI::OP_WRITE10) {
		return writeSector(blocks);
	}
	// error
	blocks = 0;
	return 0;
}

//  MBR erase only
void SCSILS120::formatUnit()
{
	if (getReady() && !checkReadOnly()) {
		memset(buffer, 0, SECTOR_SIZE);
		try {
			file.seek(0);
			file.write(buffer, SECTOR_SIZE);
			unitAttention = true;
			mediaChanged = true;
		} catch (FileException&) {
			keycode = SCSI::SENSE_WRITE_FAULT;
		}
	}
}

byte SCSILS120::getStatusCode()
{
	return keycode ? SCSI::ST_CHECK_CONDITION : SCSI::ST_GOOD;
}

void SCSILS120::eject()
{
	file.close();
	mediaChanged = true;
	if (mode & MODE_UNITATTENTION) {
		unitAttention = true;
	}
	motherBoard.getMSXCliComm().update(CliComm::MEDIA, name, "");
}

void SCSILS120::insert(string_ref filename)
{
	file = File(filename);
	mediaChanged = true;
	if (mode & MODE_UNITATTENTION) {
		unitAttention = true;
	}
	motherBoard.getMSXCliComm().update(CliComm::MEDIA, name, filename);
}

unsigned SCSILS120::executeCmd(const byte* cdb_, SCSI::Phase& phase, unsigned& blocks)
{
	memcpy(cdb, cdb_, sizeof(cdb));
	message = 0;
	phase = SCSI::STATUS;
	blocks = 0;

	// check unit attention
	if (unitAttention && (mode & MODE_UNITATTENTION) &&
	    (cdb[0] != SCSI::OP_INQUIRY) && (cdb[0] != SCSI::OP_REQUEST_SENSE)) {
		unitAttention = false;
		keycode = SCSI::SENSE_POWER_ON;
		if (cdb[0] == SCSI::OP_TEST_UNIT_READY) {
			mediaChanged = false;
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
			testUnitReady();
			return 0;

		case SCSI::OP_INQUIRY: {
			unsigned counter = inquiry();
			if (counter) {
				phase = SCSI::DATA_IN;
			}
			return counter;
		}
		case SCSI::OP_REQUEST_SENSE: {
			unsigned counter = requestSense();
			if (counter) {
				phase = SCSI::DATA_IN;
			}
			return counter;
		}
		case SCSI::OP_READ6:
			if (currentLength == 0) {
				currentLength = SECTOR_SIZE >> 1;
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
			if (currentLength == 0) {
				currentLength = SECTOR_SIZE >> 1;
			}
			if (checkAddress() && !checkReadOnly()) {
				motherBoard.getLedStatus().setLed(LedStatus::FDD, true);
				unsigned tmp = std::min(currentLength, BUFFER_BLOCK_SIZE);
				blocks = currentLength - tmp;
				unsigned counter = tmp * SECTOR_SIZE;
				cdb[0] = SCSI::OP_WRITE10;
				phase = SCSI::DATA_OUT;
				return counter;
			}
			return 0;

		case SCSI::OP_SEEK6:
			motherBoard.getLedStatus().setLed(LedStatus::FDD, true);
			currentLength = 1;
			checkAddress();
			return 0;

		case SCSI::OP_MODE_SENSE: {
			unsigned counter = modeSense();
			if (counter) {
				phase = SCSI::DATA_IN;
			}
			return counter;
		}
		case SCSI::OP_FORMAT_UNIT:
			formatUnit();
			return 0;

		case SCSI::OP_START_STOP_UNIT:
			startStopUnit();
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
				unsigned counter = readSector(blocks);
				if (counter) {
					phase = SCSI::DATA_IN;
					return counter;
				}
			}
			return 0;

		case SCSI::OP_WRITE10:
			if (checkAddress() && !checkReadOnly()) {
				unsigned tmp = std::min(currentLength, BUFFER_BLOCK_SIZE);
				blocks = currentLength - tmp;
				unsigned counter = tmp * SECTOR_SIZE;
				phase = SCSI::DATA_OUT;
				return counter;
			}
			return 0;

		case SCSI::OP_READ_CAPACITY: {
			unsigned counter = readCapacity();
			if (counter) {
				phase = SCSI::DATA_IN;
			}
			return counter;
		}
		case SCSI::OP_SEEK10:
			motherBoard.getLedStatus().setLed(LedStatus::FDD, true);
			currentLength = 1;
			checkAddress();
			return 0;
		}
	}

	// unsupported command
	keycode = SCSI::SENSE_INVALID_COMMAND_CODE;
	return 0;
}

unsigned SCSILS120::executingCmd(SCSI::Phase& phase, unsigned& blocks)
{
	phase = SCSI::EXECUTE;
	blocks = 0;
	return 0; // we're very fast
}

byte SCSILS120::msgIn()
{
	byte result = message;
	message = 0;
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
int SCSILS120::msgOut(byte value)
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

size_t SCSILS120::getNbSectorsImpl() const
{
	return file.is_open() ? (const_cast<File&>(file).getSize() / SECTOR_SIZE) : 0;
}

bool SCSILS120::isWriteProtectedImpl() const
{
	return false;
}

Sha1Sum SCSILS120::getSha1SumImpl(FilePool& filePool)
{
	if (hasPatches()) {
		return SectorAccessibleDisk::getSha1SumImpl(filePool);
	}
	return filePool.getSha1Sum(file);
}

void SCSILS120::readSectorImpl(size_t sector, SectorBuffer& buf)
{
	file.seek(sizeof(buf) * sector);
	file.read(&buf, sizeof(buf));
}

void SCSILS120::writeSectorImpl(size_t sector, const SectorBuffer& buf)
{
	file.seek(sizeof(buf) * sector);
	file.write(&buf, sizeof(buf));
}

SectorAccessibleDisk* SCSILS120::getSectorAccessibleDisk()
{
	return this;
}

const std::string& SCSILS120::getContainerName() const
{
	return name;
}

bool SCSILS120::diskChanged()
{
	return mediaChanged; // TODO not reset on read
}

int SCSILS120::insertDisk(string_ref filename)
{
	try {
		insert(filename);
		return 0;
	} catch (MSXException&) {
		return -1;
	}
}


// class LSXCommand

LSXCommand::LSXCommand(CommandController& commandController_,
                       StateChangeDistributor& stateChangeDistributor_,
                       Scheduler& scheduler_, SCSILS120& ls_)
	: RecordedCommand(commandController_, stateChangeDistributor_,
	                  scheduler_, ls_.name)
	, ls(ls_)
{
}

void LSXCommand::execute(array_ref<TclObject> tokens, TclObject& result,
                         EmuTime::param /*time*/)
{
	if (tokens.size() == 1) {
		auto& file = ls.file;
		result.addListElement(ls.name + ':');
		result.addListElement(file.is_open() ? file.getURL() : "");
		if (!file.is_open()) result.addListElement("empty");
	} else if ((tokens.size() == 2) &&
	           ((tokens[1] == "eject") || (tokens[1] == "-eject"))) {
		ls.eject();
		// TODO check for locked tray
		if (tokens[1] == "-eject") {
			result.setString(
				"Warning: use of '-eject' is deprecated, "
				"instead use the 'eject' subcommand");
		}
	} else if ((tokens.size() == 2) ||
	           ((tokens.size() == 3) && (tokens[1] == "insert"))) {
		int fileToken = 1;
		if (tokens[1] == "insert") {
			if (tokens.size() > 2) {
				fileToken = 2;
			} else {
				throw CommandException(
					"Missing argument to insert subcommand");
			}
		}
		try {
			string filename = userFileContext().resolve(
				tokens[fileToken].getString().str());
			ls.insert(filename);
			// return filename; // Note: the diskX command doesn't do this either, so this has not been converted to TclObject style here
		} catch (FileException& e) {
			throw CommandException("Can't change disk image: " +
					e.getMessage());
		}
	} else {
		throw CommandException("Too many or wrong arguments.");
	}
}

string LSXCommand::help(const vector<string>& /*tokens*/) const
{
	return ls.name + "                   : display the disk image for this LS-120 drive\n" +
	       ls.name + " eject             : eject the disk image from this LS-120 drive\n" +
	       ls.name + " insert <filename> : change the disk image for this LS-120 drive\n" +
	       ls.name + " <filename>        : change the disk image for this LS-120 drive\n";
}

void LSXCommand::tabCompletion(vector<string>& tokens) const
{
	static const char* const extra[] = { "eject", "insert" };
	completeFileName(tokens, userFileContext(), extra);
}


template<typename Archive>
void SCSILS120::serialize(Archive& ar, unsigned /*version*/)
{
	string filename = file.is_open() ? file.getURL() : "";
	ar.serialize("filename", filename);
	if (ar.isLoader()) {
		// re-insert disk before restoring 'mediaChanged'
		if (filename.empty()) {
			eject();
		} else {
			insert(filename);
		}
	}

	ar.serialize("keycode", keycode);
	ar.serialize("currentSector", currentSector);
	ar.serialize("currentLength", currentLength);
	ar.serialize("unitAttention", unitAttention);
	ar.serialize("mediaChanged", mediaChanged);
	ar.serialize("message", message);
	ar.serialize("lun", lun);
	ar.serialize_blob("cdb", cdb, sizeof(cdb));
}
INSTANTIATE_SERIALIZE_METHODS(SCSILS120);
REGISTER_POLYMORPHIC_INITIALIZER(SCSIDevice, SCSILS120, "SCSILS120");

} // namespace openmsx
