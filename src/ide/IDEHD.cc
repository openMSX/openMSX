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

	buffer[0x01] = 0x0C; // HD

	buffer[0x02] = cylinders & 0xFF;
	buffer[0x03] = cylinders / 0x100;
	buffer[0x06] = heads & 0xFF;
	buffer[0x07] = heads / 0x100;
	buffer[0x0C] = sectors & 0xFF;
	buffer[0x0D] = sectors / 0x100;

	buffer[0x78] = (totalSectors & 0x000000FF) >>  0;
	buffer[0x79] = (totalSectors & 0x0000FF00) >>  8;
	buffer[0x7A] = (totalSectors & 0x00FF0000) >> 16;
	buffer[0x7B] = (totalSectors & 0xFF000000) >> 24;
}

void IDEHD::readBlockStart(byte* buffer)
{
	try {
		readLogicalSector(transferSectorNumber, buffer);
		transferSectorNumber++;
	} catch (FileException &e) {
		abortReadTransfer(0x40);
	}
}

void IDEHD::writeBlockComplete(byte* buffer)
{
	try {
		writeLogicalSector(transferSectorNumber, buffer);
		transferSectorNumber++;
	} catch (FileException &e) {
		abortWriteTransfer(0x40);
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
			setError(0x10 | ABORT);
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
