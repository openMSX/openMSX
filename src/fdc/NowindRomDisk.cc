#include "NowindRomDisk.hh"
#include "serialize.hh"
#include "serialize_meta.hh"

namespace openmsx {

SectorAccessibleDisk* NowindRomDisk::getSectorAccessibleDisk()
{
	return nullptr;
}

std::string_view NowindRomDisk::getContainerName() const
{
	return "NowindRomDisk";
}

bool NowindRomDisk::diskChanged()
{
	return false;
}

int NowindRomDisk::insertDisk(const std::string& /*filename*/)
{
	return -1; // Can't change NowindRomDisk disk image
}

void NowindRomDisk::serialize(Archive auto& /*ar*/, unsigned /*version*/)
{
}
INSTANTIATE_SERIALIZE_METHODS(NowindRomDisk);
REGISTER_POLYMORPHIC_CLASS(DiskContainer, NowindRomDisk, "NowindRomDisk");

} // namespace openmsx
