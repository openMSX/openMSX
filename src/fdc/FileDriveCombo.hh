// $Id$

#ifndef FILEDRIVECOMBO_HH
#define FILEDRIVECOMBO_HH

#include "SectorAccessibleDisk.hh"
#include "DiskContainer.hh"
#include <string>
#include <memory>

namespace openmsx {

class File;

class FileDriveCombo : public SectorAccessibleDisk, public DiskContainer
{
public:
	FileDriveCombo(const std::string& filename);

	// SectorAccessibleDisk
	virtual void readLogicalSector(unsigned sector, byte* buf);
	virtual void writeLogicalSector(unsigned sector, const byte* buf);
	virtual unsigned getNbSectors() const;

	// DiskContainer
	SectorAccessibleDisk* getDisk();

private:
	std::auto_ptr<File> file;
};

} // namespace openmsx

#endif
