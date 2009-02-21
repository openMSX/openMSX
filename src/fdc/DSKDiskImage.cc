// $Id$

#include "DSKDiskImage.hh"
#include "File.hh"

namespace openmsx {

DSKDiskImage::DSKDiskImage(const Filename& fileName)
	: SectorBasedDisk(fileName)
	, file(new File(fileName, File::PRE_CACHE))
{
	setNbSectors(file->getSize() / SECTOR_SIZE);
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

bool DSKDiskImage::isWriteProtectedImpl() const
{
	return file->isReadOnly();
}

} // namespace openmsx
