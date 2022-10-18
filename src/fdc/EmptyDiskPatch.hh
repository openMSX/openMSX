#ifndef EMPTYDISKPATCH_HH
#define EMPTYDISKPATCH_HH

#include "PatchInterface.hh"

namespace openmsx {

class SectorAccessibleDisk;

class EmptyDiskPatch final : public PatchInterface
{
public:
	explicit EmptyDiskPatch(SectorAccessibleDisk& disk);

	void copyBlock(size_t src, uint8_t* dst, size_t num) const override;
	[[nodiscard]] size_t getSize() const override;
	[[nodiscard]] std::vector<Filename> getFilenames() const override;
	[[nodiscard]] bool isEmptyPatch() const override { return true; }

private:
	SectorAccessibleDisk& disk;
};

} // namespace openmsx

#endif
