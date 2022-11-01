#ifndef DISKCONTAINER_HH
#define DISKCONTAINER_HH

#include "serialize_meta.hh"
#include <functional>
#include <string>
#include <string_view>

namespace openmsx {

class SectorAccessibleDisk;
class MSXMotherBoard;

class DiskContainer
{
public:
	DiskContainer(const DiskContainer&) = delete;
	DiskContainer& operator=(const DiskContainer&) = delete;

	DiskContainer() = default;
	virtual ~DiskContainer() = default;

	[[nodiscard]] virtual SectorAccessibleDisk* getSectorAccessibleDisk() = 0;
	[[nodiscard]] virtual std::string_view getContainerName() const = 0;
	virtual bool diskChanged() = 0;

	// for nowind
	//  - error handling with return values instead of exceptions
	virtual int insertDisk(const std::string& filename) = 0;
	// for nowind
	[[nodiscard]] bool isRomDisk() const;

	template<typename Archive>
	void serialize(Archive& /*ar*/, unsigned /*version*/) {}
};

// Subclass 'DiskChanger' needs (global) 'MSXMotherBoard' constructor parameter
REGISTER_BASE_CLASS_1(DiskContainer, "DiskContainer", std::reference_wrapper<MSXMotherBoard>);

} // namespace openmsx

#endif
