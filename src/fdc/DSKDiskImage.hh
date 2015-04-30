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
	DSKDiskImage(const Filename& filename, const std::shared_ptr<File>& file);
	~DSKDiskImage();

private:
	void readSectorImpl (size_t sector,       SectorBuffer& buf) override;
	void writeSectorImpl(size_t sector, const SectorBuffer& buf) override;
	bool isWriteProtectedImpl() const override;
	Sha1Sum getSha1SumImpl(FilePool& filepool) override;

	const std::shared_ptr<File> file;
};

} // namespace openmsx

#endif
