// $Id$

#ifndef __MSXDISKROMPATCH_HH__
#define __MSXDISKROMPATCH_HH__

#include "MSXRomPatchInterface.hh"

class MSXDiskRomPatch: public MSXRomPatchInterface
{
	public:
		virtual void patch(int address);
};

#endif // __MSXDISKROMPATCH_HH__
