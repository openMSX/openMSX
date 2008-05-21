// $Id$

#include "IDEHD.hh"
#include "FileException.hh"
#include "MSXMotherBoard.hh"
#include "DiskManipulator.hh"
#include <cassert>

namespace openmsx {

IDEHD::IDEHD(MSXMotherBoard& motherBoard, const XMLElement& config,
             const EmuTime& /*time*/)
	: HD(motherBoard, config)
	, AbstractIDEDevice(motherBoard)
	, diskManipulator(motherBoard.getDiskManipulator())
{
	diskManipulator.registerDrive(*this);
}

IDEHD::~IDEHD()
{
	diskManipulator.unregisterDrive(*this);
}

bool IDEHD::isPacketDevice()
{
	return false;
}

const std::string& IDEHD::getDeviceName()
{
	static const std::string NAME = "OPENMSX HARD DISK";
	return NAME;
}

void IDEHD::fillIdentifyBlock(byte* buffer)
{
	unsigned totalSectors = getNbSectors();
	word heads = 16;
	word sectors = 32;
	word cylinders = totalSectors / (heads * sectors);
	buffer[1 * 2 + 0] = cylinders & 0xFF;
	buffer[1 * 2 + 1] = cylinders >> 8;
	buffer[3 * 2 + 0] = heads & 0xFF;
	buffer[3 * 2 + 1] = heads >> 8;
	buffer[6 * 2 + 0] = sectors & 0xFF;
	buffer[6 * 2 + 1] = sectors >> 8;

	buffer[47 * 2 + 0] = 16; // max sector transfer per interrupt
	buffer[47 * 2 + 1] = 0x80; // specced value

	// .... 1...: IORDY supported (hardware signal used by PIO modes >3)
	// .... ..1.: LBA supported
	buffer[49 * 2 + 1] = 0x0A;

	buffer[60 * 2 + 0] = (totalSectors & 0x000000FF) >>  0;
	buffer[60 * 2 + 1] = (totalSectors & 0x0000FF00) >>  8;
	buffer[61 * 2 + 0] = (totalSectors & 0x00FF0000) >> 16;
	buffer[61 * 2 + 1] = (totalSectors & 0xFF000000) >> 24;
}

unsigned IDEHD::readBlockStart(byte* buffer, unsigned count)
{
	try {
		assert(count >= 512);
		(void)count; // avoid warning
		readSector(transferSectorNumber, buffer);
		++transferSectorNumber;
		return 512;
	} catch (FileException& e) {
		abortReadTransfer(UNC);
		return 0;
	}
}

void IDEHD::writeBlockComplete(byte* buffer, unsigned count)
{
	try {
		while (count != 0) {
			writeSector(transferSectorNumber, buffer);
			++transferSectorNumber;
			assert(count >= 512);
			count -= 512;
		}
	} catch (FileException& e) {
		abortWriteTransfer(UNC);
	}
}

void IDEHD::executeCommand(byte cmd)
{
	switch (cmd) {
	case 0x20: // Read Sector
	case 0x30: { // Write Sector
		unsigned sectorNumber = getSectorNumber();
		unsigned numSectors = getNumSectors();
		if ((sectorNumber + numSectors) > getNbSectors()) {
			// Note: The original code set ABORT as well, but that is not
			//       allowed according to the spec.
			setError(IDNF);
			break;
		}
		transferSectorNumber = sectorNumber;
		if (cmd == 0x20) {
			startLongReadTransfer(numSectors * 512);
		} else {
			startWriteTransfer(numSectors * 512);
		}
		break;
	}

	case 0xF8: // Read Native Max Address
		// We don't support the Host Protected Area feature set, but SymbOS
		// uses only this particular command, so we support just this one.
		setSectorNumber(getNbSectors());
		break;

	default: // all others
		AbstractIDEDevice::executeCommand(cmd);
	}
}

void IDEHD::readSector(unsigned sector, byte* buf)
{
	readFromImage(512 * sector, 512, buf);
}

void IDEHD::writeSector(unsigned sector, const byte* buf)
{
	writeToImage(512 * sector, 512, buf);
}

unsigned IDEHD::getNbSectors() const
{
	return getImageSize() / 512;
}

SectorAccessibleDisk* IDEHD::getSectorAccessibleDisk()
{
	return this;
}

const std::string& IDEHD::getContainerName() const
{
	return getName();
}

} // namespace openmsx
