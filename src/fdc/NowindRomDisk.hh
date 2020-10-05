#ifndef NOWINDROMDISK_HH
#define NOWINDROMDISK_HH

#include "DiskContainer.hh"

namespace openmsx {

class NowindRomDisk final : public DiskContainer
{
public:
	SectorAccessibleDisk* getSectorAccessibleDisk() override;
	const std::string& getContainerName() const override;
	bool diskChanged() override;
	int insertDisk(std::string_view filename) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);
};

} // namespace openmsx

#endif
