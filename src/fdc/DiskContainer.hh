// $Id$

#ifndef DISKCONTAINER_HH
#define DISKCONTAINER_HH

#include <string>

namespace openmsx {

class SectorAccessibleDisk;

class DiskContainer
{
public:
	virtual SectorAccessibleDisk* getSectorAccessibleDisk() = 0;
	virtual const std::string& getContainerName() const = 0;

protected:
	virtual ~DiskContainer();
};

} // namespace openmsx

#endif
