#include "DiskContainer.hh"
#include "NowindRomDisk.hh"

namespace openmsx {

DiskContainer::~DiskContainer()
{
}

bool DiskContainer::isRomdisk() const
{
	return dynamic_cast<const NowindRomDisk*>(this) != nullptr;
}

} // namespace openmsx
