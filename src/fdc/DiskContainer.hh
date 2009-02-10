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
	virtual bool diskChanged() = 0;

	// for nowind
	//  - error handling with return values instead of exceptions
	virtual int insertDisk(const std::string& filename) = 0;

protected:
	virtual ~DiskContainer();
};

} // namespace openmsx

#endif
