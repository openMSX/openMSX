#ifndef DSKDISKIMAGE_HH
#define DSKDISKIMAGE_HH

#include "SectorBasedDisk.hh"
#include <memory>

namespace openmsx {

class File;

class DSKDiskImage : public SectorBasedDisk
{
public:
	explicit DSKDiskImage(const Filename& filename);
	DSKDiskImage(const Filename& filename, const std::shared_ptr<File>& file);
	virtual ~DSKDiskImage();

private:
	virtual void readSectorImpl(size_t sector, byte* buf);
	virtual void writeSectorImpl(size_t sector, const byte* buf);
	virtual bool isWriteProtectedImpl() const;
	virtual Sha1Sum getSha1Sum();

	const std::shared_ptr<File> file;
};

} // namespace openmsx

#endif
