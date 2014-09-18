#ifndef EMPTYDISKPATCH_HH
#define EMPTYDISKPATCH_HH

#include "PatchInterface.hh"

namespace openmsx {

class SectorAccessibleDisk;

class EmptyDiskPatch final : public PatchInterface
{
public:
	explicit EmptyDiskPatch(SectorAccessibleDisk& disk);

	virtual void copyBlock(size_t src, byte* dst, size_t num) const;
	virtual size_t getSize() const;
	virtual std::vector<Filename> getFilenames() const;
	virtual bool isEmptyPatch() const { return true; }

private:
	SectorAccessibleDisk& disk;
};

} // namespace openmsx

#endif
