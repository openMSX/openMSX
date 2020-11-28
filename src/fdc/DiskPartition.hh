#ifndef DISKPARTITION_HH
#define DISKPARTITION_HH

#include "SectorBasedDisk.hh"
#include <memory>

namespace openmsx {

class DiskPartition final : public SectorBasedDisk
{
public:
	/** Return a partition (as a SectorbasedDisk) from another Disk.
	 * @param disk The whole disk.
	 * @param partition The partition number.
	 *        0 for the whole disk
	 *        1-31 for a specific partition, this must be a valid partition.
	 * @param owned If specified it should be a shared_ptr to the Disk
	 *              object passed as first parameter. This DiskPartition
	 *              will then take (shared) ownership of that Disk.
	 */
	DiskPartition(SectorAccessibleDisk& disk, unsigned partition,
	              std::shared_ptr<SectorAccessibleDisk> owned = nullptr);

	DiskPartition(SectorAccessibleDisk& parent,
	              size_t start, size_t length);

private:
	void readSectorImpl (size_t sector,       SectorBuffer& buf) override;
	void writeSectorImpl(size_t sector, const SectorBuffer& buf) override;
	[[nodiscard]] bool isWriteProtectedImpl() const override;

private:
	SectorAccessibleDisk& parent;
	std::shared_ptr<SectorAccessibleDisk> owned;
	size_t start;
};

} // namespace openmsx

#endif
