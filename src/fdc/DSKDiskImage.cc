// $Id$

#include "DSKDiskImage.hh"

namespace openmsx {

DSKDiskImage::DSKDiskImage(const string& fileName)
	: file(fileName)
{
	nbSectors = file.getSize() / SECTOR_SIZE;
}

DSKDiskImage::~DSKDiskImage()
{
}

void DSKDiskImage::read(byte track, byte sector, byte side,
                   int size, byte* buf)
{
	try {
		int logicalSector = physToLog(track, side, sector);
		PRT_DEBUG("Reading sector : " << logicalSector );
		if (logicalSector >= nbSectors) {
			throw NoSuchSectorException("No such sector");
		}
		file.seek(logicalSector * SECTOR_SIZE);
		file.read(buf, SECTOR_SIZE);
	} catch (FileException &e) {
		throw DiskIOErrorException("Disk I/O error");
	}
}

void DSKDiskImage::write(byte track, byte sector, byte side, 
                    int size, const byte* buf)
{
	if (writeProtected()) {
		throw WriteProtectedException("");
	}
	try {
		int logicalSector = physToLog(track, side, sector);
		if (logicalSector >= nbSectors) {
			throw NoSuchSectorException("No such sector");
		}
		file.seek(logicalSector * SECTOR_SIZE);
		file.write(buf, SECTOR_SIZE);
	} catch (FileException &e) {
		throw DiskIOErrorException("Disk I/O error");
	}
}

void DSKDiskImage::readBootSector()
{
	// These are just heuristics, so they are not perfect. We may need
	// a way to overrule this (or use a more descriptive disk format).
	if (nbSectors >= 1440) {
		sectorsPerTrack = 9;
		nbSides = 2;
	} else if (nbSectors == 720) {
		sectorsPerTrack = 9;
		nbSides = 1;
	} else {
		Disk::readBootSector();
	}
}

bool DSKDiskImage::writeProtected()
{
	return file.isReadOnly();
}

bool DSKDiskImage::doubleSided()
{
	return nbSides == 2;
}

} // namespace openmsx
