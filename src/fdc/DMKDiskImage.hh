#ifndef DMKDISKIMAGE_HH
#define DMKDISKIMAGE_HH

#include "Disk.hh"
#include <memory>

namespace openmsx {

class File;

/** DMK disk image. A description of the file format can be found
  *   in doc/DMK-Format-Details.txt or at the original site:
  *   http://www.trs-80.com/wordpress/dsk-and-dmk-image-utilities/
  *   (at the bottom of the page)
  */
class DMKDiskImage final : public Disk
{
public:
	DMKDiskImage(Filename filename, std::shared_ptr<File> file);

	void readTrack(byte track, byte side, RawTrack& output) override;
	void writeTrackImpl(byte track, byte side, const RawTrack& input) override;

	// logical sector emulation for SectorAccessibleDisk
	void readSectorImpl (size_t sector,       SectorBuffer& buf) override;
	void writeSectorImpl(size_t sector, const SectorBuffer& buf) override;
	[[nodiscard]] size_t getNbSectorsImpl() const override;
	[[nodiscard]] bool isWriteProtectedImpl() const override;
	[[nodiscard]] Sha1Sum getSha1SumImpl(FilePool& filepool) override;

private:
	void detectGeometryFallback() override;

	void seekTrack(byte track, byte side);
	void doWriteTrack(byte track, byte side, const RawTrack& input);
	void extendImageToTrack(byte track);

private:
	std::shared_ptr<File> file;
	unsigned numTracks;
	unsigned dmkTrackLen;
	bool singleSided;
	bool writeProtected;
};

} // namespace openmsx

#endif
