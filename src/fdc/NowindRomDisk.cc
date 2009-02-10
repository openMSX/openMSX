// $Id$

#include "NowindRomDisk.hh"

namespace openmsx {

SectorAccessibleDisk* NowindRomDisk::getSectorAccessibleDisk()
{
	return NULL;
}

const std::string& NowindRomDisk::getContainerName() const
{
	static const std::string NAME = "NowindRomDisk";
	return NAME;
}

bool NowindRomDisk::diskChanged()
{
	return false;
}

int NowindRomDisk::insertDisk(const std::string& filename)
{
	return -1; // Can't change NowindRomDisk disk image
}

} // namespace openmsx
