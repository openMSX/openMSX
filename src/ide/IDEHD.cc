// $Id$

#include "IDEHD.hh"
#include "File.hh"
#include "FileContext.hh"
#include "FileException.hh"
#include "XMLElement.hh"
#include <cassert>

using std::string;

namespace openmsx {

IDEHD::IDEHD(EventDistributor& eventDistributor_, const XMLElement& config,
             const EmuTime& time)
	: AbstractIDEDevice(eventDistributor_, time)
{
	const string& filename = config.getChildData("filename");
	file.reset(new File(config.getFileContext().resolveCreate(filename),
	                    File::CREATE));

	unsigned wantedSize = config.getChildDataAsInt("size");	// in MB
	wantedSize *= 1024 * 1024;
	unsigned fileSize = file->getSize();
	if (wantedSize > fileSize) {
		// for safety only enlarge file
		file->truncate(wantedSize);
	}

	totalSectors = wantedSize / 512;
}

IDEHD::~IDEHD()
{
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

void IDEHD::readBlockStart(byte* buffer)
{
	try {
		readLogicalSector(transferSectorNumber, buffer);
		transferSectorNumber++;
	} catch (FileException &e) {
		abortReadTransfer(UNC);
	}
}

void IDEHD::writeBlockComplete(byte* buffer)
{
	try {
		writeLogicalSector(transferSectorNumber, buffer);
		transferSectorNumber++;
	} catch (FileException &e) {
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
		if ((sectorNumber + numSectors) > totalSectors) {
			// Note: The original code set ABORT as well, but that is not
			//       allowed according to the spec.
			setError(IDNF);
			break;
		}
		transferSectorNumber = sectorNumber;
		if (cmd == 0x20) {
			startReadTransfer(512/2 * numSectors);
		} else {
			startWriteTransfer(512/2 * numSectors);
		}
		break;
	}
	default: // all others
		AbstractIDEDevice::executeCommand(cmd);
	}
}

void IDEHD::readLogicalSector(unsigned sector, byte* buf)
{
	file->seek(512 * sector);
	file->read(buf, 512);
}

void IDEHD::writeLogicalSector(unsigned sector, const byte* buf)
{
	file->seek(512 * sector);
	file->write(buf, 512);
}

unsigned IDEHD::getNbSectors() const
{
	return file->getSize() / 512;
}

SectorAccessibleDisk* IDEHD::getSectorAccessibleDisk()
{
	return this;
}

} // namespace openmsx
