#include "DSKDiskImage.hh"
#include "File.hh"
#include "FilePool.hh"

namespace openmsx {

DSKDiskImage::DSKDiskImage(const Filename& fileName)
	: SectorBasedDisk(fileName)
	, file(std::make_shared<File>(fileName, File::PRE_CACHE))
{
	setNbSectors(file->getSize() / sizeof(SectorBuffer));
}

DSKDiskImage::DSKDiskImage(const Filename& fileName,
                           std::shared_ptr<File> file_)
	: SectorBasedDisk(fileName)
	, file(std::move(file_))
{
	setNbSectors(file->getSize() / sizeof(SectorBuffer));
}

void DSKDiskImage::readSectorsImpl(
	SectorBuffer* buffers, size_t startSector, size_t num)
{
	file->seek(startSector * sizeof(SectorBuffer));
	file->read(buffers, num * sizeof(SectorBuffer));
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

Sha1Sum DSKDiskImage::getSha1SumImpl(FilePool& filePool)
{
	if (hasPatches()) {
		return SectorAccessibleDisk::getSha1SumImpl(filePool);
	}
	return filePool.getSha1Sum(*file);
}

} // namespace openmsx
