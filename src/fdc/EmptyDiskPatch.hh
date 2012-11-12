// $Id$

#ifndef EMPTYDISKPATCH_HH
#define EMPTYDISKPATCH_HH

#include "PatchInterface.hh"

namespace openmsx {

class SectorAccessibleDisk;

class EmptyDiskPatch : public PatchInterface
{
public:
	explicit EmptyDiskPatch(SectorAccessibleDisk& disk);

	virtual void copyBlock(unsigned src, byte* dst, unsigned num) const;
	virtual unsigned getSize() const;
	virtual std::vector<Filename> getFilenames() const;
	virtual bool isEmptyPatch() const { return true; }

private:
	SectorAccessibleDisk& disk;
};

} // namespace openmsx

#endif
