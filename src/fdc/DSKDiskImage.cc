// $Id$

#include "DSKDiskImage.hh"
#include "File.hh"
#include "FileException.hh"

using std::string;

namespace openmsx {

DSKDiskImage::DSKDiskImage(const string& fileName)
	: file(new File(fileName))
{
	nbSectors = file->getSize() / SECTOR_SIZE;
}

DSKDiskImage::~DSKDiskImage()
{
}

void DSKDiskImage::read(byte track, byte sector, byte side,
                   int /*size*/, byte* buf)
{
	try {
		int logicalSector = physToLog(track, side, sector);
		PRT_DEBUG("Reading sector : " << logicalSector );
		if (logicalSector >= nbSectors) {
			throw NoSuchSectorException("No such sector");
		}
		file->seek(logicalSector * SECTOR_SIZE);
		file->read(buf, SECTOR_SIZE);
	} catch (FileException &e) {
		throw DiskIOErrorException("Disk I/O error");
	}
}

void DSKDiskImage::write(byte track, byte sector, byte side, 
                    int /*size*/, const byte* buf)
{
	if (writeProtected()) {
		throw WriteProtectedException("");
	}
	try {
		int logicalSector = physToLog(track, side, sector);
		if (logicalSector >= nbSectors) {
			throw NoSuchSectorException("No such sector");
		}
		file->seek(logicalSector * SECTOR_SIZE);
		file->write(buf, SECTOR_SIZE);
	} catch (FileException &e) {
		throw DiskIOErrorException("Disk I/O error");
	}
}

bool DSKDiskImage::writeProtected()
{
	return file->isReadOnly();
}

bool DSKDiskImage::doubleSided()
{
	return nbSides == 2;
}

} // namespace openmsx
