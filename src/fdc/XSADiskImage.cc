#include "XSADiskImage.hh"

#include "DiskExceptions.hh"
#include "XSAExtractor.hh"

#include "File.hh"

namespace openmsx {

XSADiskImage::XSADiskImage(const Filename& filename, File& file)
	: SectorBasedDisk(DiskName(filename))
{
	XSAExtractor extractor(file.mmap());
	data = std::move(extractor).extractData();
	setNbSectors(data.size());
}

void XSADiskImage::readSectorsImpl(
	std::span<SectorBuffer> buffers, size_t startSector)
{
	ranges::copy(std::span{&data[startSector], buffers.size()}, buffers);
}

void XSADiskImage::writeSectorImpl(size_t /*sector*/, const SectorBuffer& /*buf*/)
{
	throw WriteProtectedException("Write protected");
}

bool XSADiskImage::isWriteProtectedImpl() const
{
	return true;
}

} // namespace openmsx
