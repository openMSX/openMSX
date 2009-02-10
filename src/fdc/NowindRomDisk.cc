// $Id$

#include "NowindRomDisk.hh"
#include "MSXException.hh"

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

void NowindRomDisk::insertDisk(const std::string& filename)
{
	throw MSXException("Can't change NowindRomDisk disk image");
}

} // namespace openmsx
