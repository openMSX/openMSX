// $Id$

#include "AbstractIDEDevice.hh"
#include "EventDistributor.hh"
#include "LedEvent.hh"
#include "Version.hh"
#include <cassert>
#include <string.h>

namespace openmsx {

AbstractIDEDevice::AbstractIDEDevice(
	EventDistributor& eventDistributor_, const EmuTime& /*time*/
	)
	: eventDistributor(eventDistributor_)
{
	buffer = new byte[512];
	transferRead = transferWrite = false;
}

AbstractIDEDevice::~AbstractIDEDevice()
{
	delete[] buffer;
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

void AbstractIDEDevice::reset(const EmuTime& /*time*/)
{
	errorReg = diagnostic();
	statusReg = DRDY | DSC;
	featureReg = 0x00;
	createSignature();
	setTransferRead(false);
	setTransferWrite(false);
}

byte AbstractIDEDevice::readReg(nibble reg, const EmuTime& /*time*/)
{
	switch (reg) {
	case 1:	// error register
		return errorReg;

	case 2:	// sector count register
		return sectorCountReg;

	case 3:	// sector number register
		return sectorNumReg;

	case 4:	// cyclinder low register
		return cylinderLowReg;

	case 5:	// cyclinder high register
		return cylinderHighReg;

	case 6:	// device/head register
		// DEV bit is handled by IDE interface
		return devHeadReg;

	case 7:	// status register
		return statusReg;

	case 8:
	case 9:
	case 10:
	case 11:
	case 12:
	case 13:
	case 15:// not used
		return 0x7F;

	case 0:	// data register, converted to readData by IDE interface
	case 14:// alternate status reg, converted to read from normal
		// status register by IDE interface
	default:
		assert(false);
		return 0x7F;	// avoid warning
	}
}

void AbstractIDEDevice::writeReg(
	nibble reg, byte value, const EmuTime& /*time*/
	)
{
	switch (reg) {
	case 1: // feature register
		featureReg = value;
		break;

	case 2: // sector count register
		sectorCountReg = value;
		break;

	case 3: // sector number register
		sectorNumReg = value;
		break;

	case 4: // cyclinder low register
		cylinderLowReg = value;
		break;

	case 5: // cyclinder high register
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
		transferIdentifyBlock = false;
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
		assert(false);
		break;
	}
}

word AbstractIDEDevice::readData(const EmuTime& /*time*/)
{
	if (!transferRead) {
		// no read in progress
		return 0x7F7F;
	}
	if ((transferCount & 255) == 0) {
		if (transferIdentifyBlock) {
			createIdentifyBlock();
		} else {
			readBlockStart(buffer);
		}
		transferPntr = buffer;
	}
	word result = transferPntr[0] + (transferPntr[1] << 8);
	transferPntr += 2;
	transferCount--;
	if (transferCount == 0) {
		// everything read
		setTransferRead(false);
		statusReg &= ~DRQ;
	}
	return result;
}

void AbstractIDEDevice::writeData(word value, const EmuTime& /*time*/)
{
	if (!transferWrite) {
		// no write in progress
		return;
	}
	transferPntr[0] = value & 0xFF;
	transferPntr[1] = value >> 8;
	transferPntr += 2;
	transferCount--;
	if ((transferCount & 255) == 0) {
		writeBlockComplete(buffer);
		transferPntr = buffer;
	}
	if (transferCount == 0) {
		// everything written
		setTransferWrite(false);
		statusReg &= ~DRQ;
	}
}

void AbstractIDEDevice::setError(byte error)
{
	errorReg = error;
	statusReg |= ERR;
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
			transferIdentifyBlock = true;
			startReadTransfer(512/2);
		} else {
			setError(ABORT);
		}
		break;

	case 0xEC: // Identify Device
		if (isPacketDevice()) {
			setError(ABORT);
		} else {
			transferIdentifyBlock = true;
			startReadTransfer(512/2);
		}
		break;

	case 0xEF: // Set Features
		switch (featureReg) {
		case 0x03: // Set Transfer Mode
			break;
		default:
			setError(ABORT);
		}
		break;

	default: // unsupported command
		//fprintf(stderr, "IDE command %02X\n", cmd);
		setError(ABORT);
	}
}

void AbstractIDEDevice::startReadTransfer(unsigned count)
{
	transferCount = count;
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
	transferPntr = buffer;
	transferCount = count;
	statusReg |= DRQ;
	setTransferWrite(true);
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
			eventDistributor.distributeEvent(
				new LedEvent(LedEvent::FDD, transferRead)
				);
		}
	}
}

void AbstractIDEDevice::setTransferWrite(bool status)
{
	if (status != transferWrite) {
		transferWrite = status;
		if (!transferRead) {
			// (this is a bit of a hack!)
			eventDistributor.distributeEvent(
				new LedEvent(LedEvent::FDD, transferWrite)
				);
		}
	}
}

void AbstractIDEDevice::writeIdentifyString(
	byte* p, unsigned len, const std::string& s
) {
	std::string::const_iterator it = s.begin();
	for (unsigned count = 0; count < len; count++) {
		char c1, c2;
		if (it == s.end()) { c1 = ' '; } else { c1 = *it++; }
		if (it == s.end()) { c2 = ' '; } else { c2 = *it++; }
		*p++ = c2;
		*p++ = c1;
	}
}

void AbstractIDEDevice::createIdentifyBlock()
{
	memset(buffer, 0x00, 512);

	// According to the spec, the combination of model and serial should be
	// unique. But I don't know any MSX software that cares about this.
	writeIdentifyString(&buffer[10 * 2], 10, "s00000001"); // serial
	writeIdentifyString(&buffer[23 * 2], 4,
		// Use openMSX version as firmware revision, because most of our
		// IDE emulation code is in fact emulating the firmware.
		Version::RELEASE
		? "v" + Version::VERSION
		: "d" + Version::CHANGELOG_REVISION
		);
	writeIdentifyString(&buffer[27 * 2], 20, getDeviceName()); // model

	fillIdentifyBlock(buffer);
}

} // namespace openmsx
