#include "AbstractIDEDevice.hh"
#include "MSXMotherBoard.hh"
#include "LedStatus.hh"
#include "Version.hh"
#include "serialize.hh"
#include "unreachable.hh"
#include "xrange.hh"
#include <cassert>
#include <cstring>
#include <cstdio>

namespace openmsx {

AbstractIDEDevice::AbstractIDEDevice(MSXMotherBoard& motherBoard_)
	: motherBoard(motherBoard_)
	, transferIdx(0) // avoid UMR on serialize
	, bufferLeft(0)
	, transferCount(0)
	, transferRead(false)
	, transferWrite(false)
{
	memset(buffer, 0, sizeof(buffer));
}

byte AbstractIDEDevice::diagnostic()
{
	// The Execute Device Diagnostic command is executed by both devices in
	// parallel. Fortunately, returning 0x01 is valid in all cases:
	// - for device 0 it means: device 0 passed, device 1 passed or not present
	// - for device 1 it means: device 1 passed
	return 0x01;
}

void AbstractIDEDevice::createSignature(bool preserveDevice)
{
	sectorCountReg = 0x01;
	sectorNumReg = 0x01;
	if (isPacketDevice()) {
		cylinderLowReg = 0x14;
		cylinderHighReg = 0xEB;
		if (preserveDevice) {
			devHeadReg &= 0x10;
		} else {
			// The current implementation of SunriseIDE will substitute the
			// current device for DEV, so it will always act like DEV is
			// preserved.
			devHeadReg = 0x00;
		}
	} else {
		cylinderLowReg = 0x00;
		cylinderHighReg = 0x00;
		devHeadReg = 0x00;
	}
}

void AbstractIDEDevice::reset(EmuTime::param /*time*/)
{
	errorReg = diagnostic();
	statusReg = DRDY | DSC;
	featureReg = 0x00;
	createSignature();
	setTransferRead(false);
	setTransferWrite(false);
}

byte AbstractIDEDevice::readReg(nibble reg, EmuTime::param /*time*/)
{
	switch (reg) {
	case 1: // error register
		return errorReg;

	case 2: // sector count register
		return sectorCountReg;

	case 3: // sector number register / LBA low
		return sectorNumReg;

	case 4: // cyclinder low register / LBA mid
		return cylinderLowReg;

	case 5: // cyclinder high register / LBA high
		return cylinderHighReg;

	case 6: // device/head register
		// DEV bit is handled by IDE interface
		return devHeadReg;

	case 7: // status register
		return statusReg;

	case 8:
	case 9:
	case 10:
	case 11:
	case 12:
	case 13:
	case 15:// not used
		return 0x7F;

	case 0: // data register, converted to readData by IDE interface
	case 14:// alternate status reg, converted to read from normal
		// status register by IDE interface
	default:
		UNREACHABLE; return 0x7F; // avoid warning
	}
}

void AbstractIDEDevice::writeReg(
	nibble reg, byte value, EmuTime::param /*time*/
	)
{
	switch (reg) {
	case 1: // feature register
		featureReg = value;
		break;

	case 2: // sector count register
		sectorCountReg = value;
		break;

	case 3: // sector number register / LBA low
		sectorNumReg = value;
		break;

	case 4: // cyclinder low register / LBA mid
		cylinderLowReg = value;
		break;

	case 5: // cyclinder high register / LBA high
		cylinderHighReg = value;
		break;

	case 6: // device/head register
		// DEV bit is handled by IDE interface
		devHeadReg = value;
		break;

	case 7: // command register
		statusReg &= ~(DRQ | ERR);
		setTransferRead(false);
		setTransferWrite(false);
		executeCommand(value);
		break;

	case 8:
	case 9:
	case 10:
	case 11:
	case 12:
	case 13:
	case 15: // not used
	case 14: // device control register, handled by IDE interface
		// do nothing
		break;

	case 0: // data register, converted to readData by IDE interface
	default:
		UNREACHABLE; break;
	}
}

word AbstractIDEDevice::readData(EmuTime::param /*time*/)
{
	if (!transferRead) {
		// no read in progress
		return 0x7F7F;
	}
	assert((transferIdx + 1) < sizeof(buffer));
	word result = (buffer[transferIdx + 0] << 0) +
	              (buffer[transferIdx + 1] << 8);
	transferIdx += 2;
	bufferLeft -= 2;
	if (bufferLeft == 0) {
		if (transferCount == 0) {
			// End of transfer.
			setTransferRead(false);
			statusReg &= ~DRQ;
			readEnd();
		} else {
			// Buffer empty, but transfer not done yet.
			readNextBlock();
		}
	}
	return result;
}

void AbstractIDEDevice::readNextBlock()
{
	bufferLeft = readBlockStart(
		buffer, std::min<unsigned>(sizeof(buffer), transferCount));
	assert((bufferLeft & 1) == 0);
	transferIdx = 0;
	transferCount -= bufferLeft;
}

void AbstractIDEDevice::writeData(word value, EmuTime::param /*time*/)
{
	if (!transferWrite) {
		// no write in progress
		return;
	}
	assert((transferIdx + 1) < sizeof(buffer));
	buffer[transferIdx + 0] = value & 0xFF;
	buffer[transferIdx + 1] = value >> 8;
	transferIdx += 2;
	bufferLeft -= 2;
	if (bufferLeft == 0) {
		unsigned bytesInBuffer = transferIdx;
		if (transferCount == 0) {
			// End of transfer.
			setTransferWrite(false);
			statusReg &= ~DRQ;
		} else {
			// Buffer full, but transfer not done yet.
			writeNextBlock();
		}
		// Packet commands can start a second transfer, so the command
		// execution must happen after we close this transfer.
		writeBlockComplete(buffer, bytesInBuffer);
	}
}

void AbstractIDEDevice::writeNextBlock()
{
	transferIdx = 0;
	bufferLeft = std::min<unsigned>(sizeof(buffer), transferCount);
	transferCount -= bufferLeft;
}

void AbstractIDEDevice::setError(byte error)
{
	errorReg = error;
	if (error) {
		statusReg |= ERR;
	} else {
		statusReg &= ~ERR;
	}
	statusReg &= ~DRQ;
	setTransferWrite(false);
	setTransferRead(false);
}

unsigned AbstractIDEDevice::getSectorNumber() const
{
	return sectorNumReg | (cylinderLowReg << 8) |
		(cylinderHighReg << 16) | ((devHeadReg & 0x0F) << 24);
}

unsigned AbstractIDEDevice::getNumSectors() const
{
	return (sectorCountReg == 0) ? 256 : sectorCountReg;
}

void AbstractIDEDevice::setInterruptReason(byte value)
{
	sectorCountReg = value;
}

unsigned AbstractIDEDevice::getByteCount() const
{
	return cylinderLowReg | (cylinderHighReg << 8);
}

void AbstractIDEDevice::setByteCount(unsigned count)
{
	cylinderLowReg = count & 0xFF;
	cylinderHighReg = count >> 8;
}

void AbstractIDEDevice::setSectorNumber(unsigned lba)
{
	sectorNumReg    = (lba & 0x000000FF) >>  0;
	cylinderLowReg  = (lba & 0x0000FF00) >>  8;
	cylinderHighReg = (lba & 0x00FF0000) >> 16;
	devHeadReg      = (lba & 0x0F000000) >> 24; // note: only 4 bits
}

void AbstractIDEDevice::readEnd()
{
}

void AbstractIDEDevice::executeCommand(byte cmd)
{
	switch (cmd) {
	case 0x08: // Device Reset
		if (isPacketDevice()) {
			errorReg = diagnostic();
			createSignature(true);
			// TODO: Which is correct?
			//statusReg = 0x00;
			statusReg = DRDY | DSC;
		} else {
			// Command is only implemented for packet devices.
			setError(ABORT);
		}
		break;

	case 0x90: // Execute Device Diagnostic
		errorReg = diagnostic();
		createSignature();
		break;

	case 0x91: // Initialize Device Parameters
		// ignore command
		break;

	case 0xA1: // Identify Packet Device
		if (isPacketDevice()) {
			createIdentifyBlock(startShortReadTransfer(512));
		} else {
			setError(ABORT);
		}
		break;

	case 0xEC: // Identify Device
		if (isPacketDevice()) {
			setError(ABORT);
		} else {
			createIdentifyBlock(startShortReadTransfer(512));
		}
		break;

	case 0xEF: // Set Features
		switch (getFeatureReg()) {
		case 0x03: // Set Transfer Mode
			break;
		default:
			fprintf(stderr, "Unhandled set feature subcommand: %02X\n",
			        getFeatureReg());
			setError(ABORT);
		}
		break;

	default: // unsupported command
		fprintf(stderr, "unsupported IDE command %02X\n", cmd);
		setError(ABORT);
	}
}

AlignedBuffer& AbstractIDEDevice::startShortReadTransfer(unsigned count)
{
	assert(count <= sizeof(buffer));
	assert((count & 1) == 0);

	startReadTransfer();
	transferCount = 0;
	bufferLeft = count;
	transferIdx = 0;
	memset(buffer, 0x00, count);
	return buffer;
}

void AbstractIDEDevice::startLongReadTransfer(unsigned count)
{
	assert((count & 1) == 0);
	startReadTransfer();
	transferCount = count;
	readNextBlock();
}

void AbstractIDEDevice::startReadTransfer()
{
	statusReg |= DRQ;
	setTransferRead(true);
}

void AbstractIDEDevice::abortReadTransfer(byte error)
{
	setError(error | ABORT);
	setTransferRead(false);
}

void AbstractIDEDevice::startWriteTransfer(unsigned count)
{
	statusReg |= DRQ;
	setTransferWrite(true);
	transferCount = count;
	writeNextBlock();
}

void AbstractIDEDevice::abortWriteTransfer(byte error)
{
	setError(error | ABORT);
	setTransferWrite(false);
}

void AbstractIDEDevice::setTransferRead(bool status)
{
	if (status != transferRead) {
		transferRead = status;
		if (!transferWrite) {
			// (this is a bit of a hack!)
			motherBoard.getLedStatus().setLed(LedStatus::FDD, transferRead);
		}
	}
}

void AbstractIDEDevice::setTransferWrite(bool status)
{
	if (status != transferWrite) {
		transferWrite = status;
		if (!transferRead) {
			// (this is a bit of a hack!)
			motherBoard.getLedStatus().setLed(LedStatus::FDD, transferWrite);
		}
	}
}

/** Writes a string to a location in the identify block.
  * Helper method for createIdentifyBlock.
  * @param p Pointer to write the characters to.
  * @param len Number of words to write.
  * @param s ASCII string to write.
  *   If the string is longer  than len*2 characters, it is truncated.
  *   If the string is shorter than len*2 characters, it is padded with spaces.
  */
static void writeIdentifyString(byte* p, unsigned len, std::string s)
{
	s.resize(2 * len, ' ');
	for (auto i : xrange(len)) {
		// copy and swap
		p[2 * i + 0] = s[2 * i + 1];
		p[2 * i + 1] = s[2 * i + 0];
	}
}

void AbstractIDEDevice::createIdentifyBlock(AlignedBuffer& buf)
{
	// According to the spec, the combination of model and serial should be
	// unique. But I don't know any MSX software that cares about this.
	writeIdentifyString(&buf[10 * 2], 10, "s00000001"); // serial
	writeIdentifyString(&buf[23 * 2], 4,
		// Use openMSX version as firmware revision, because most of our
		// IDE emulation code is in fact emulating the firmware.
		Version::RELEASE ? strCat('v', Version::VERSION)
		                 : strCat('d', Version::REVISION));
	writeIdentifyString(&buf[27 * 2], 20, std::string(getDeviceName())); // model

	fillIdentifyBlock(buf);
}


void AbstractIDEDevice::serialize(Archive auto& ar, unsigned /*version*/)
{
	// no need to serialize IDEDevice base class
	ar.serialize_blob("buffer", buffer, sizeof(buffer));
	ar.serialize("transferIdx",     transferIdx,
	             "bufferLeft",      bufferLeft,
	             "transferCount",   transferCount,
	             "errorReg",        errorReg,
	             "sectorCountReg",  sectorCountReg,
	             "sectorNumReg",    sectorNumReg,
	             "cylinderLowReg",  cylinderLowReg,
	             "cylinderHighReg", cylinderHighReg,
	             "devHeadReg",      devHeadReg,
	             "statusReg",       statusReg,
	             "featureReg",      featureReg);
	bool transferIdentifyBlock = false; // remove on next version increment
	                                    // no need to break bw-compat now
	ar.serialize("transferIdentifyBlock", transferIdentifyBlock,
	             "transferRead",          transferRead,
	             "transferWrite",         transferWrite);
}
INSTANTIATE_SERIALIZE_METHODS(AbstractIDEDevice);

} // namespace openmsx
