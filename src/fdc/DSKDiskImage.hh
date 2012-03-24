// $Id$

#ifndef DSKDISKIMAGE_HH
#define DSKDISKIMAGE_HH

#include "SectorBasedDisk.hh"
#include "shared_ptr.hh"

namespace openmsx {

class File;

class DSKDiskImage : public SectorBasedDisk
{
public:
	explicit DSKDiskImage(const Filename& filename);
	DSKDiskImage(const Filename& filename, shared_ptr<File> file);
	virtual ~DSKDiskImage();

private:
	virtual void readSectorImpl(unsigned sector, byte* buf);
	virtual void writeSectorImpl(unsigned sector, const byte* buf);
	virtual bool isWriteProtectedImpl() const;
	virtual std::string getSha1Sum();

	const shared_ptr<File> file;
};

} // namespace openmsx

#endif
