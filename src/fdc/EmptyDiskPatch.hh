// $Id$

#ifndef EMPTYDISKPATCH_HH
#define EMPTYDISKPATCH_HH

#include "PatchInterface.hh"

namespace openmsx {

class SectorBasedDisk;

class EmptyDiskPatch : public PatchInterface
{
public:
	explicit EmptyDiskPatch(SectorBasedDisk& disk);

	virtual void copyBlock(unsigned src, byte* dst, unsigned num) const;
	virtual unsigned getSize() const;
	virtual void getFilenames(std::vector<Filename>& result) const;

private:
	SectorBasedDisk& disk;
};

} // namespace openmsx

#endif
