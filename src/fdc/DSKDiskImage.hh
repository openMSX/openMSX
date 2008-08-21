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
	virtual ~DSKDiskImage();

private:
	virtual void readSectorSBD(unsigned sector, byte* buf);
	virtual void writeSectorSBD(unsigned sector, const byte* buf);
	virtual bool isWriteProtectedImpl() const;

	std::auto_ptr<File> file;
};

} // namespace openmsx

#endif
