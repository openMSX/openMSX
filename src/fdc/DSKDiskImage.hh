// $Id$

#ifndef DSKDISKIMAGE_HH
#define DSKDISKIMAGE_HH

#include "SectorBasedDisk.hh"
#include <memory>

namespace openmsx {

class File;
class FilePool;

class DSKDiskImage : public SectorBasedDisk
{
public:
	DSKDiskImage(const Filename& filename, FilePool& filepool);
	virtual ~DSKDiskImage();

private:
	virtual void readSectorImpl(unsigned sector, byte* buf);
	virtual void writeSectorImpl(unsigned sector, const byte* buf);
	virtual bool isWriteProtectedImpl() const;
	virtual std::string getSha1Sum();

	const std::auto_ptr<File> file;
};

} // namespace openmsx

#endif
