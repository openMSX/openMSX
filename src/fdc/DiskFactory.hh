#ifndef DISKFACTORY_HH
#define DISKFACTORY_HH

#include "DirAsDSK.hh"
#include "EnumSetting.hh"
#include <string>

namespace openmsx {

class Reactor;
class DiskChanger;
class Disk;

class DiskFactory
{
public:
	explicit DiskFactory(Reactor& reactor);
	[[nodiscard]] std::unique_ptr<Disk> createDisk(
		const std::string& diskImage, DiskChanger& diskChanger);

private:
	Reactor& reactor;
	EnumSetting<DirAsDSK::SyncMode> syncDirAsDSKSetting;
	EnumSetting<DirAsDSK::BootSectorType> bootSectorSetting;
};

} // namespace openmsx

#endif // DISKFACTORY_HH
