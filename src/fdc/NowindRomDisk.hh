#ifndef NOWINDROMDISK_HH
#define NOWINDROMDISK_HH

#include "DiskContainer.hh"

namespace openmsx {

class NowindRomDisk final : public DiskContainer
{
public:
	virtual SectorAccessibleDisk* getSectorAccessibleDisk();
	virtual const std::string& getContainerName() const;
	virtual bool diskChanged();
	virtual int insertDisk(string_ref filename);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);
};

} // namespace openmsx

#endif
