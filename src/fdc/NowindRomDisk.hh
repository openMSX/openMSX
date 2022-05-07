#ifndef NOWINDROMDISK_HH
#define NOWINDROMDISK_HH

#include "DiskContainer.hh"

namespace openmsx {

class NowindRomDisk final : public DiskContainer
{
public:
	[[nodiscard]] SectorAccessibleDisk* getSectorAccessibleDisk() override;
	[[nodiscard]] std::string_view getContainerName() const override;
	[[nodiscard]] bool diskChanged() override;
	int insertDisk(const std::string& filename) override;

	void serialize(Archive auto& ar, unsigned version);
};

} // namespace openmsx

#endif
