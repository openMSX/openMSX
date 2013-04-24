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

	virtual void copyBlock(size_t src, byte* dst, size_t num) const = 0;
	virtual size_t getSize() const = 0;
	virtual std::vector<Filename> getFilenames() const = 0;
	virtual bool isEmptyPatch() const { return false; }
};

} // namespace openmsx

#endif
