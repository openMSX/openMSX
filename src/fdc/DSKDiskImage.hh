// $Id$

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
	DSKDiskImage(const Filename& filename, std::auto_ptr<File> file);
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
