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

void DSKDiskImage::readLogicalSector(unsigned sector, byte* buf)
{
	file->seek(sector * SECTOR_SIZE);
	file->read(buf, SECTOR_SIZE);
}

void DSKDiskImage::write(byte track, byte sector, byte side, 
                         unsigned /*size*/, const byte* buf)
{
	if (writeProtected()) {
		throw WriteProtectedException("");
	}
	try {
		unsigned logicalSector = physToLog(track, side, sector);
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

} // namespace openmsx
