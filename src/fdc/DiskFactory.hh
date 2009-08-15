// $Id$

#ifndef DISKFACTORY_HH
#define DISKFACTORY_HH

#include <string>

namespace openmsx {

class CommandController;
class Disk;

class DiskFactory
{
public:
	explicit DiskFactory(CommandController& controller);
	Disk* createDisk(const std::string& diskImage);

private:
	CommandController& controller;
};

} // namespace openmsx

#endif // DISKFACTORY_HH
