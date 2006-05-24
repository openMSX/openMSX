// $Id$

#include "IDECDROM.hh"
#include <cassert>

namespace openmsx {

IDECDROM::IDECDROM(
	CommandController& /*commandController*/,
	EventDistributor& eventDistributor_,
	const XMLElement& /*config*/,
	const EmuTime& time
	)
	: AbstractIDEDevice(eventDistributor_, time)
{
}

IDECDROM::~IDECDROM()
{
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
}

void IDECDROM::readBlockStart(byte* /*buffer*/)
{
	// No read transfers are supported yet.
	assert(false);
}

void IDECDROM::writeBlockComplete(byte* buffer)
{
	// Currently, packet writes are the only kind of write transfer.
	executePacketCommand(buffer);
}

void IDECDROM::executeCommand(byte cmd)
{
	switch (cmd) {
	case 0xA0: // Packet Command (ATAPI)
		fprintf(stderr, "ATAPI Command\n");
		startWriteTransfer(12/2);
		break;

	case 0xDA: // ATA Get Media Status
		fprintf(stderr, "ATA Get Media Status\n");
		setError(ABORT);
		break;

	default: // all others
		AbstractIDEDevice::executeCommand(cmd);
	}
}

void IDECDROM::executePacketCommand(byte* packet)
{
	// It seems that unlike ATA which uses words at the basic data unit,
	// ATAPI uses bytes.
	fprintf(stderr, "ATAPI Packet:");
	for (unsigned i = 0; i < sizeof(packet); i++) {
		fprintf(stderr, " %02X", packet[i]);
	}
	fprintf(stderr, "\n");

	switch (packet[0]) {
	case 0x03: {
		int count = packet[4];
		fprintf(stderr, "  request sense: %d bytes\n", count);
		setError(ABORT);
		break;
	}
	case 0x43: {
		bool msf = packet[1] & 2;
		int format = packet[2] & 0x0F;
		int trackOrSession = packet[6];
		int allocLen = (packet[7] << 8) | packet[8];
		//int control = packet[9];
		switch (format) {
		case 0: { // TOC
			fprintf(stderr, "  read TOC: %s addressing, "
				"start track %d, allocation length 0x%04X\n",
				msf ? "MSF" : "logical block",
				trackOrSession,
				allocLen
				);
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
	case 0xA8: {
		int sectorNumber = (packet[2] << 24) | (packet[3] << 16)
		                 | (packet[4] <<  8) |  packet[5];
		int sectorCount = (packet[6] << 24) | (packet[7] << 16)
		                | (packet[8] <<  8) |  packet[9];
		fprintf(stderr,
			"  read(12): sector %d, count %d\n",
			sectorNumber, sectorCount
			);
		setError(ABORT);
		break;
	}
	default:
		fprintf(stderr, "  unknown command 0x%02X\n", packet[0]);
		setError(ABORT);
	}
}

} // namespace openmsx
