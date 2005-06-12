// $Id$

#ifndef PATCHINTERFACE_HH
#define PATCHINTERFACE_HH

#include "openmsx.hh"

namespace openmsx {

class PatchInterface
{
public:
	virtual ~PatchInterface() {}

	virtual void copyBlock(unsigned src, byte* dst, unsigned num) const = 0;
	virtual unsigned getSize() const = 0;
};

} // namespace openmsx

#endif
