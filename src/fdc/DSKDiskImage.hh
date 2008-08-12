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

	virtual bool writeProtected();

private:
	virtual void readSectorImpl(unsigned sector, byte* buf);
	virtual void writeSectorImpl(unsigned sector, const byte* buf);

	std::auto_ptr<File> file;
};

} // namespace openmsx

#endif
