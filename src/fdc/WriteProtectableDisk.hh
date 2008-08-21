// $Id$

#ifndef WRITEPROTECTABLEDISK_HH
#define WRITEPROTECTABLEDISK_HH

#include "openmsx.hh"

namespace openmsx {

class WriteProtectableDisk
{
public:
	WriteProtectableDisk()
		: forcedWriteProtect(false)
	{
	}
	bool isWriteProtected() const
	{
		return forcedWriteProtect || isWriteProtectedImpl();
	}
	void forceWriteProtect()
	{
		// can't be undone
		forcedWriteProtect = true;
	}

protected:
	virtual ~WriteProtectableDisk() {}
	virtual bool isWriteProtectedImpl() const = 0;

private:
	bool forcedWriteProtect;
};

} // namespace openmsx

#endif
