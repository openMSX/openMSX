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
	DSKDiskImage(const std::string& fileName);
	virtual ~DSKDiskImage();

	virtual void write(byte track, byte sector,
	                   byte side, unsigned size, const byte* buf);
	virtual bool writeProtected();

private:
	virtual void readLogicalSector(unsigned sector, byte* buf);

	std::auto_ptr<File> file;
};

} // namespace openmsx

#endif
