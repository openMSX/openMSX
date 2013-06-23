#include "DSKDiskImage.hh"
#include "File.hh"

namespace openmsx {

DSKDiskImage::DSKDiskImage(const Filename& fileName)
	: SectorBasedDisk(fileName)
	, file(std::make_shared<File>(fileName, File::PRE_CACHE))
{
	setNbSectors(file->getSize() / sizeof(SectorBuffer));
}

DSKDiskImage::DSKDiskImage(const Filename& fileName,
                           const std::shared_ptr<File>& file_)
	: SectorBasedDisk(fileName)
	, file(file_)
{
	setNbSectors(file->getSize() / sizeof(SectorBuffer));
}

DSKDiskImage::~DSKDiskImage()
{
}

void DSKDiskImage::readSectorImpl(size_t sector, SectorBuffer& buf)
{
	file->seek(sector * sizeof(buf));
	file->read(&buf, sizeof(buf));
}

void DSKDiskImage::writeSectorImpl(size_t sector, const SectorBuffer& buf)
{
	file->seek(sector * sizeof(buf));
	file->write(&buf, sizeof(buf));
}

bool DSKDiskImage::isWriteProtectedImpl() const
{
	return file->isReadOnly();
}

Sha1Sum DSKDiskImage::getSha1Sum()
{
	if (hasPatches()) {
		return SectorAccessibleDisk::getSha1Sum();
	}
	return file->getSha1Sum();
}

} // namespace openmsx
