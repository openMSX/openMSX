#include "AbstractIDEDevice.hh"

#include "LedStatus.hh"
#include "MSXMotherBoard.hh"
#include "Version.hh"

#include "narrow.hh"
#include "serialize.hh"
#include "unreachable.hh"

#include <algorithm>
#include <cassert>
#include <cstdio>

namespace openmsx {

AbstractIDEDevice::AbstractIDEDevice(MSXMotherBoard& motherBoard_)
	: motherBoard(motherBoard_)
{
	std::ranges::fill(buffer, 0);
}

uint8_t AbstractIDEDevice::diagnostic() const
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

void AbstractIDEDevice::reset(EmuTime /*time*/)
{
	errorReg = diagnostic();
	statusReg = DRDY | DSC;
	featureReg = 0x00;
	createSignature();
	setTransferRead(false);
	setTransferWrite(false);
}

uint8_t AbstractIDEDevice::readReg(uint4_t reg, EmuTime /*time*/)
{
	switch (reg) {
	case 1: // error register
		return errorReg;

	case 2: // sector count register
		return sectorCountReg;

	case 3: // sector number register / LBA low
		return sectorNumReg;

	case 4: // cylinder low register / LBA mid
		return cylinderLowReg;

	case 5: // cylinder high register / LBA high
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
		UNREACHABLE;
	}
}

void AbstractIDEDevice::writeReg(
	uint4_t reg, uint8_t value, EmuTime /*time*/
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

	case 4: // cylinder low register / LBA mid
		cylinderLowReg = value;
		break;

	case 5: // cylinder high register / LBA high
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
		UNREACHABLE;
	}
}

uint16_t AbstractIDEDevice::readData(EmuTime /*time*/)
{
	if (!transferRead) {
		// no read in progress
		return 0x7F7F;
	}
	assert((transferIdx + 1) < sizeof(buffer));
	auto result = uint16_t((buffer[transferIdx + 0] << 0) +
	                       (buffer[transferIdx + 1] << 8));
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

void AbstractIDEDevice::writeData(uint16_t value, EmuTime /*time*/)
{
	if (!transferWrite) {
		// no write in progress
		return;
	}
	assert((transferIdx + 1) < sizeof(buffer));
	buffer[transferIdx + 0] = narrow_cast<uint8_t>(value & 0xFF);
	buffer[transferIdx + 1] = narrow_cast<uint8_t>(value >> 8);
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

void AbstractIDEDevice::setError(uint8_t error)
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

void AbstractIDEDevice::setInterruptReason(uint8_t value)
{
	sectorCountReg = value;
}

unsigned AbstractIDEDevice::getByteCount() const
{
	return cylinderLowReg | (cylinderHighReg << 8);
}

void AbstractIDEDevice::setByteCount(unsigned count)
{
	cylinderLowReg  = narrow_cast<uint8_t>(count & 0xFF);
	cylinderHighReg = narrow_cast<uint8_t>(count >> 8);
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

void AbstractIDEDevice::executeCommand(uint8_t cmd)
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
	std::ranges::fill(std::span{buffer.data(), count}, 0);
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

void AbstractIDEDevice::abortReadTransfer(uint8_t error)
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

void AbstractIDEDevice::abortWriteTransfer(uint8_t error)
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
  * @param dst Buffer to write the characters to.
  * @param s ASCII string to write.
  *   If the string is longer  than the buffer, it is truncated.
  *   If the string is shorter than the buffer, it is padded with spaces.
  */
static void writeIdentifyString(std::span<uint8_t> dst, std::string s)
{
	assert((dst.size() % 2) == 0);
	s.resize(dst.size(), ' ');
	for (size_t i = 0; i < dst.size(); i += 2) {
		// copy and swap
		dst[i + 0] = s[i + 1];
		dst[i + 1] = s[i + 0];
	}
}

void AbstractIDEDevice::createIdentifyBlock(AlignedBuffer& buf)
{
	// According to the spec, the combination of model and serial should be
	// unique. But I don't know any MSX software that cares about this.
	writeIdentifyString(std::span{&buf[10 * 2], 2 * 10}, "s00000001"); // serial
	writeIdentifyString(std::span{&buf[23 * 2], 2 * 4},
		// Use openMSX version as firmware revision, because most of our
		// IDE emulation code is in fact emulating the firmware.
		Version::RELEASE ? strCat('v', Version::VERSION)
		                 : strCat('d', Version::REVISION));
	writeIdentifyString(std::span{&buf[27 * 2], 2 * 20}, std::string(getDeviceName())); // model

	fillIdentifyBlock(buf);
}


template<typename Archive>
void AbstractIDEDevice::serialize(Archive& ar, unsigned /*version*/)
{
	// no need to serialize IDEDevice base class
	ar.serialize_blob("buffer", buffer);
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
