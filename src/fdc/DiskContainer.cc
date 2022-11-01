#include "DiskContainer.hh"
#include "NowindRomDisk.hh"

namespace openmsx {

bool DiskContainer::isRomDisk() const
{
	return dynamic_cast<const NowindRomDisk*>(this) != nullptr;
}

} // namespace openmsx
