// $Id$

#ifndef DMKDISKIMAGE_HH
#define DMKDISKIMAGE_HH

#include "Disk.hh"
#include "shared_ptr.hh"
#include <vector>

namespace openmsx {

class File;

/** DMK disk image. See here for a description of the file format:
  *   http://www.trs-80.com/wordpress/emulation-dmk-format/
  */
class DMKDiskImage : public Disk
{
public:
	DMKDiskImage(Filename& filename, const shared_ptr<File>& file);

	virtual void readTrack(byte track, byte side, RawTrack& output);
	virtual void writeTrackImpl(byte track, byte side, const RawTrack& input);

	// logical sector emulation for SectorAccessibleDisk
	virtual void readSectorImpl(unsigned sector, byte* buf);
	virtual void writeSectorImpl(unsigned sector, const byte* buf);
	virtual unsigned getNbSectorsImpl() const;
	virtual bool isWriteProtectedImpl() const;
	virtual Sha1Sum getSha1Sum();

private:
	virtual void detectGeometryFallback();

	void seekTrack(byte track, byte side);

	shared_ptr<File> file;
	unsigned numTracks;
	unsigned dmkTrackLen;
	bool singleSided;
	bool writeProtected;
};

} // namespace openmsx

#endif
