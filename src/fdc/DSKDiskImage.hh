#ifndef DSKDISKIMAGE_HH
#define DSKDISKIMAGE_HH

#include "SectorBasedDisk.hh"
#include <memory>

namespace openmsx {

class File;

class DSKDiskImage final : public SectorBasedDisk
{
public:
	explicit DSKDiskImage(const Filename& filename);
	DSKDiskImage(const Filename& filename, std::shared_ptr<File> file);

private:
	void readSectorsImpl(
		SectorBuffer* buffers, size_t startSector, size_t num) override;
	void writeSectorImpl(size_t sector, const SectorBuffer& buf) override;
	[[nodiscard]] bool isWriteProtectedImpl() const override;
	[[nodiscard]] Sha1Sum getSha1SumImpl(FilePool& filepool) override;

private:
	const std::shared_ptr<File> file;
};

} // namespace openmsx

#endif
