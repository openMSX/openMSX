// $Id$

#ifndef PATCHINTERFACE_HH
#define PATCHINTERFACE_HH

#include "Filename.hh"
#include "openmsx.hh"
#include <vector>

namespace openmsx {

class PatchInterface
{
public:
	virtual ~PatchInterface() {}

	virtual void copyBlock(unsigned src, byte* dst, unsigned num) const = 0;
	virtual unsigned getSize() const = 0;
	virtual void getFilenames(std::vector<Filename>& result) const = 0;
	virtual bool isEmptyPatch() const { return false; }
};

} // namespace openmsx

#endif
