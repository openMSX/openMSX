#ifndef DMKDISKIMAGE_HH
#define DMKDISKIMAGE_HH

#include "Disk.hh"
#include <memory>

namespace openmsx {

class File;

/** DMK disk image. A description of the file format can be found
  *   in doc/DMK-Format-Details.txt or at the oriinal site:
  *   http://www.trs-80.com/wordpress/dsk-and-dmk-image-utilities/
  *   (at the bottom of the page)
  */
class DMKDiskImage : public Disk
{
public:
	DMKDiskImage(Filename& filename, const std::shared_ptr<File>& file);

	virtual void readTrack(byte track, byte side, RawTrack& output);
	virtual void writeTrackImpl(byte track, byte side, const RawTrack& input);

	// logical sector emulation for SectorAccessibleDisk
	virtual void readSectorImpl (size_t sector,       SectorBuffer& buf);
	virtual void writeSectorImpl(size_t sector, const SectorBuffer& buf);
	virtual size_t getNbSectorsImpl() const;
	virtual bool isWriteProtectedImpl() const;
	virtual Sha1Sum getSha1Sum();

private:
	virtual void detectGeometryFallback();

	void seekTrack(byte track, byte side);

	std::shared_ptr<File> file;
	unsigned numTracks;
	unsigned dmkTrackLen;
	bool singleSided;
	bool writeProtected;
};

} // namespace openmsx

#endif
