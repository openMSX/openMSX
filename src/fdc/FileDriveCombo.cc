// $Id$

#include "FileDriveCombo.hh"
#include "File.hh"

namespace openmsx {

FileDriveCombo::FileDriveCombo(const std::string& filename)
{
	file.reset(new File(filename, CREATE));
}

SectorAccessibleDisk& FileDriveCombo::getDisk()
{
	return *this;
}

void FileDriveCombo::readLogicalSector(unsigned sector, byte* buf)
{
	file->seek(512 * sector);
	file->read(buf, 512);
}

void FileDriveCombo::writeLogicalSector(unsigned sector, const byte* buf)
{
	file->seek(512 * sector);
	file->write(buf, 512);
}

unsigned FileDriveCombo::getNbSectors() const 
{
	return file->getSize() / 512;
}

} // namespace openmsx
