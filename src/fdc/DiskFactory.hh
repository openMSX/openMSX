// $Id$

#ifndef DISKFACTORY_HH
#define DISKFACTORY_HH

#include "DirAsDSK.hh"
#include <string>

namespace openmsx {

class CommandController;
class Disk;
template <class T> class EnumSetting;

class DiskFactory
{
public:
	explicit DiskFactory(CommandController& controller);
	Disk* createDisk(const std::string& diskImage);

private:
	CommandController& controller;
	std::auto_ptr<EnumSetting<DirAsDSK::BootSectorType> > bootSectorSetting;
	std::auto_ptr<EnumSetting<DirAsDSK::SyncMode> > syncDirAsDSKSetting;
};

} // namespace openmsx

#endif // DISKFACTORY_HH
