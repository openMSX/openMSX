#ifndef EMPTYDISKPATCH_HH
#define EMPTYDISKPATCH_HH

#include "PatchInterface.hh"

namespace openmsx {

class SectorAccessibleDisk;

class EmptyDiskPatch final : public PatchInterface
{
public:
	explicit EmptyDiskPatch(SectorAccessibleDisk& disk);

	void copyBlock(size_t src, byte* dst, size_t num) const override;
	size_t getSize() const override;
	std::vector<Filename> getFilenames() const override;
	bool isEmptyPatch() const override { return true; }

private:
	SectorAccessibleDisk& disk;
};

} // namespace openmsx

#endif
