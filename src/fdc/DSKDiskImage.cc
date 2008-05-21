// $Id$

#include "DSKDiskImage.hh"
#include "File.hh"

using std::string;

namespace openmsx {

DSKDiskImage::DSKDiskImage(const string& fileName)
	: SectorBasedDisk(fileName)
	, file(new File(fileName, File::PRE_CACHE))
{
	nbSectors = file->getSize() / SECTOR_SIZE;
}

DSKDiskImage::~DSKDiskImage()
{
}

void DSKDiskImage::readSectorImpl(unsigned sector, byte* buf)
{
	file->seek(sector * SECTOR_SIZE);
	file->read(buf, SECTOR_SIZE);
}

void DSKDiskImage::writeSectorImpl(unsigned sector, const byte* buf)
{
	file->seek(sector * SECTOR_SIZE);
	file->write(buf, SECTOR_SIZE);
}

bool DSKDiskImage::writeProtected()
{
	return file->isReadOnly();
}

} // namespace openmsx
