// $Id$

#include "IDECDROM.hh"
#include "MSXMotherBoard.hh"
#include "File.hh"
#include "FileContext.hh"
#include "FileException.hh"
#include "Command.hh"
#include "CommandException.hh"
#include <algorithm>
#include <bitset>
#include <cassert>

using std::string;
using std::vector;

namespace openmsx {

class CDXCommand : public SimpleCommand
{
public:
	CDXCommand(CommandController& commandController, IDECDROM& cd);
	virtual string execute(const vector<string>& tokens);
	virtual string help(const vector<string>& tokens) const;
	virtual void tabCompletion(vector<string>& tokens) const;
private:
	IDECDROM& cd;
};


static const unsigned MAX_CD = 26;
static std::bitset<MAX_CD> cdInUse;

static string calcName()
{
	unsigned id = 0;
	while (cdInUse[id]) {
		++id;
		if (id == MAX_CD) {
			throw MSXException("Too many CDs");
		}
	}
	return string("cd") + char('a' + id);
}

IDECDROM::IDECDROM(MSXMotherBoard& motherBoard, const XMLElement& /*config*/,
                   const EmuTime& time)
	: AbstractIDEDevice(motherBoard.getEventDistributor(), time)
	, name(calcName())
	, cdxCommand(new CDXCommand(motherBoard.getCommandController(), *this))
{
	cdInUse[name[2] - 'a'] = true;

	senseKey = 0;

	remMedStatNotifEnabled = false;
	mediaChanged = false;
}

IDECDROM::~IDECDROM()
{
	cdInUse[name[2] - 'a'] = false;
}

bool IDECDROM::isPacketDevice()
{
	return true;
}

const std::string& IDECDROM::getDeviceName()
{
	static const std::string NAME = "OPENMSX CD-ROM";
	return NAME;
}

void IDECDROM::fillIdentifyBlock(byte* buffer)
{
	// 1... ....: removable media
	// .10. ....: fast handling of packet command (immediate, in fact)
	// .... .1..: incomplete response:
	//            fields that depend on medium are undefined
	// .... ..00: support for 12-byte packets
	buffer[0 * 2 + 0] = 0xC4;
	// 10.. ....: ATAPI
	// ...0 0101: CD-ROM device
	buffer[0 * 2 + 1] = 0x85;

	// ...1 ....: Removable Media Status Notification feature set supported
	buffer[ 83 * 2 + 0] = 0x10;
	// ...1 ....: Removable Media Status Notification feature set enabled
	buffer[ 86 * 2 + 0] = remMedStatNotifEnabled * 0x10;
	// .... ..01: Removable Media Status Notification feature set supported (again??)
	buffer[127 * 2 + 0] = 0x01;
}

unsigned IDECDROM::readBlockStart(byte* buffer, unsigned count)
{
	assert(readSectorData);
	if (file.get()) {
		//fprintf(stderr, "read sector data at %08X\n", transferOffset);
		file->seek(transferOffset);
		file->read(buffer, count);
		transferOffset += count;
		return count;
	} else {
		//fprintf(stderr, "read sector failed: no medium\n");
		// TODO: Check whether more error flags should be set.
		abortReadTransfer(0);
		return 0;
	}
}

void IDECDROM::readEnd()
{
	setInterruptReason(I_O | C_D);
}

void IDECDROM::writeBlockComplete(byte* buffer, unsigned count)
{
	// Currently, packet writes are the only kind of write transfer.
	assert(count == 12);
	(void)count; // avoid warning
	executePacketCommand(buffer);
}

void IDECDROM::executeCommand(byte cmd)
{
	switch (cmd) {
	case 0xA0: // Packet Command (ATAPI)
		// Determine packet size for data packets.
		byteCountLimit = getByteCount();
		//fprintf(stderr, "ATAPI Command, byte count limit %04X\n",
		//	byteCountLimit);

		// Prepare to receive the command.
		startWriteTransfer(12);
		setInterruptReason(C_D);
		break;

	case 0xDA: // ATA Get Media Status
		if (remMedStatNotifEnabled) {
			setError(0);
		} else {
			// na WP MC na MCR ABRT NM obs
			byte err = 0;
			if (file.get()) {
				err |= 0x40; // WP (write protected)
			} else {
				err |= 0x02; // NM (no media inserted)
			}
			// MCR (media change request) is not yet supported
			if (mediaChanged) {
				err |= 0x20; // MC (media changed)
				mediaChanged = false;
			}
			//fprintf(stderr, "Get Media status: %02X\n", err);
			setError(err);
		}
		break;

	case 0xEF: // Set Features
		switch (getFeatureReg()) {
		case 0x31: // Disable Media Status Notification.
			remMedStatNotifEnabled = false;
			break;
		case 0x95: // Enable  Media Status Notification
			setLBAMid(0x00); // version
			// .... .0..: capable of physically ejecting media
			// .... ..0.: capable of locking the media
			// .... ...X: previous enabled state
			setLBAHigh(remMedStatNotifEnabled);
			remMedStatNotifEnabled = true;
			break;
		default: // other subcommands handled by base class
			AbstractIDEDevice::executeCommand(cmd);
		}
		break;

	default: // all others
		AbstractIDEDevice::executeCommand(cmd);
	}
}

void IDECDROM::startPacketReadTransfer(unsigned count)
{
	// TODO: Recompute for each packet.
	// TODO: Take even/odd stuff into account.
	// Note: The spec says maximum byte count is 0xFFFE, but I prefer
	//       powers of two, so I'll use 0x8000 instead (the device is
	//       free to set limitations of its own).
	unsigned packetSize = 512; /*std::min(
		byteCountLimit, // limit from user
		std::min(sizeof(buffer), 0x8000u) // device and spec limit
		);*/
	unsigned size = std::min(packetSize, count);
	setByteCount(size);
	setInterruptReason(I_O);
}

void IDECDROM::executePacketCommand(byte* packet)
{
	// It seems that unlike ATA which uses words at the basic data unit,
	// ATAPI uses bytes.
	//fprintf(stderr, "ATAPI Packet:");
	//for (unsigned i = 0; i < 12; i++) {
	//	fprintf(stderr, " %02X", packet[i]);
	//}
	//fprintf(stderr, "\n");

	readSectorData = false;

	switch (packet[0]) {
	case 0x03: { // REQUEST SENSE Command
		// TODO: Find out what the purpose of the allocation length is.
		//       In practice, it seems to be 18, which is the amount we want
		//       to return, but what if it would be different?
		//int allocationLength = packet[4];
		//fprintf(stderr, "  request sense: %d bytes\n", allocationLength);

		const int byteCount = 18;
		startPacketReadTransfer(byteCount);
		byte* buffer = startShortReadTransfer(byteCount);
		for (int i = 0; i < byteCount; i++) {
			buffer[i] = 0x00;
		}
		buffer[0] = 0xF0;
		buffer[2] = senseKey >> 16; // sense key
		buffer[12] = (senseKey >> 8) & 0xFF; // ASC
		buffer[13] = senseKey & 0xFF; // ASQ
		buffer[7] = byteCount - 7;
		senseKey = 0;
		break;
	}
	case 0x43: { // READ TOC/PMA/ATIP Command
		//bool msf = packet[1] & 2;
		int format = packet[2] & 0x0F;
		//int trackOrSession = packet[6];
		//int allocLen = (packet[7] << 8) | packet[8];
		//int control = packet[9];
		switch (format) {
		case 0: { // TOC
			//fprintf(stderr, "  read TOC: %s addressing, "
			//	"start track %d, allocation length 0x%04X\n",
			//	msf ? "MSF" : "logical block",
			//	trackOrSession, allocLen);
			setError(ABORT);
			break;
		}
		case 1: // Session Info
		case 2: // Full TOC
		case 3: // PMA
		case 4: // ATIP
		default:
			fprintf(stderr, "  read TOC: format %d not implemented\n", format);
			setError(ABORT);
		}
		break;
	}
	case 0xA8: { // READ Command
		int sectorNumber = (packet[2] << 24) | (packet[3] << 16)
		                 | (packet[4] <<  8) |  packet[5];
		int sectorCount = (packet[6] << 24) | (packet[7] << 16)
		                | (packet[8] <<  8) |  packet[9];
		//fprintf(stderr, "  read(12): sector %d, count %d\n",
		//	sectorNumber, sectorCount);
		// There are three block sizes:
		// - byteCountLimit: set by the host
		//     maximum block size for transfers
		// - byteCount: determined by the device
		//     actual block size for transfers
		// - transferCount wrap: emulation thingy
		//     transparent to host
		//fprintf(stderr, "byte count limit: %04X\n", byteCountLimit);
		//unsigned byteCount = sectorCount * 2048;
		//unsigned byteCount = sizeof(buffer);
		//unsigned byteCount = packetSize;
		/*
		if (byteCount > byteCountLimit) {
			byteCount = byteCountLimit;
		}
		if (byteCount > 0xFFFE) {
			byteCount = 0xFFFE;
		}
		*/
		//fprintf(stderr, "byte count: %04X\n", byteCount);
		readSectorData = true;
		transferOffset = sectorNumber * 2048;
		unsigned count = sectorCount * 2048;
		startPacketReadTransfer(count);
		startLongReadTransfer(count);
		break;
	}
	default:
		fprintf(stderr, "  unknown packet command 0x%02X\n", packet[0]);
		setError(ABORT);
	}
}

void IDECDROM::eject()
{
	file.reset();
	mediaChanged = true;
	senseKey = 0x06 << 16; // unit attention (medium changed)
}

void IDECDROM::insert(const string& filename)
{
	std::auto_ptr<File> newFile(new File(filename));
	file = newFile;
	mediaChanged = true;
	senseKey = 0x06 << 16; // unit attention (medium changed)
}


// class CDXCommand

CDXCommand::CDXCommand(CommandController& commandController, IDECDROM& cd_)
	: SimpleCommand(commandController, cd_.name)
	, cd(cd_)
{
}

string CDXCommand::execute(const vector<string>& tokens)
{
	switch (tokens.size()) {
	case 1: {
		File* file = cd.file.get();
		return file ? file->getOriginalName() : "";
	}
	case 2: {
		// TODO check for locked tray
		if (tokens[1] == "-eject") {
			cd.eject();
			return "";
		} else {
			try {
				UserFileContext context(getCommandController());
				string filename = context.resolve(tokens[1]);
				cd.insert(filename);
				return filename;
			} catch (FileException& e) {
				throw CommandException("Can't change cd image: " +
				                       e.getMessage());
			}
		}
	}
	default:
		throw CommandException("Too many arguments.");
	}
}

string CDXCommand::help(const vector<string>& /*tokens*/) const
{
	return cd.name + ": change the cd image for this CDROM";
}

void CDXCommand::tabCompletion(vector<string>& tokens) const
{
	completeFileName(tokens);
}

} // namespace openmsx
