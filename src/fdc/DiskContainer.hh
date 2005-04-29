// $Id$

#ifndef DISKCONTAINER_HH
#define DISKCONTAINER_HH

namespace openmsx {

class SectorAccessibleDisk;
  
class DiskContainer
{
public:
	virtual SectorAccessibleDisk& getDisk() = 0;

protected:
	virtual ~DiskContainer();
};

} // namespace openmsx

#endif
