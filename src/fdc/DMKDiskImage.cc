// $Id$

#include "DMKDiskImage.hh"
#include "DiskExceptions.hh"
#include "File.hh"
#include "static_assert.hh"
#include <algorithm>
#include <cassert>

using std::vector;

namespace openmsx {

struct DmkHeader
{
	byte writeProtected;
	byte numTracks;
	byte trackLen[2];
	byte flags;
	byte reserved[7];
	byte format[4];
};
STATIC_ASSERT(sizeof(DmkHeader) == 16);

static const byte FLAG_SINGLE_SIDED = 0x10;
static const unsigned IDAM_FLAGS_MASK = 0xC000;
static const unsigned FLAG_MFM_SECTOR = 0x8000;


static bool isValidDmkHeader(const DmkHeader& header)
{
	if (!((header.writeProtected == 0x00) ||
	      (header.writeProtected == 0xff))) {
		return false;
	}
	unsigned trackLen = header.trackLen[0] + 256 * header.trackLen[1];
	if (trackLen >= 0x4000) return false; // too large track length
	if (trackLen <= 128)    return false; // too small
	if (header.flags & ~0xd0) return false; // unknown flag set
	for (int i = 0; i < 7; ++i) {
		if (header.reserved[i] != 0) return false;
	}
	for (int i = 0; i < 4; ++i) {
		if (header.format[i] != 0) return false;
	}
	return true;
}

DMKDiskImage::DMKDiskImage(Filename& filename, const shared_ptr<File>& file_)
	: Disk(filename)
	, file(file_)
{
	DmkHeader header;
	file->seek(0);
	file->read(&header, sizeof(header));
	if (!isValidDmkHeader(header)) {
		throw MSXException("Not a DMK image");
	}

	numTracks = header.numTracks;
	dmkTrackLen = header.trackLen[0] + 256 * header.trackLen[1] - 128;
	singleSided = (header.flags & FLAG_SINGLE_SIDED) != 0;;
	writeProtected = header.writeProtected == 0xff;

	// TODO should we print a warning when dmkTrackLen is too far from the
	//      ideal value RawTrack::SIZE? This might indicate the disk image
	//      was not a 3.5" DD disk image and data will be lost on either
	//      read or write.
}

void DMKDiskImage::seekTrack(byte track, byte side)
{
	unsigned t = singleSided ? track : (2 * track + side);
	file->seek(sizeof(DmkHeader) + t * (dmkTrackLen + 128));
}

void DMKDiskImage::readTrack(byte track, byte side, RawTrack& output)
{
	assert(side < 2);
	output.clear(dmkTrackLen);
	if ((singleSided && side) || (track >= numTracks)) {
		// no such side/track, only clear output
		return;
	}

	seekTrack(track, side);

	// Read idam data (still needs to be converted).
	byte idamBuf[2 * 64];
	file->read(idamBuf, sizeof(idamBuf));

	// Read raw track data.
	file->read(output.getRawBuffer(), dmkTrackLen);

	// Convert idam data into an easier to work with internal format.
	int lastIdam = -1;
	for (int i = 0; i < 64; ++i) {
		unsigned idx = idamBuf[2 * i + 0] + 256 * idamBuf[2 * i + 1];
		if (idx == 0) break; // end of table reached

		if ((idx & IDAM_FLAGS_MASK) != FLAG_MFM_SECTOR) {
			// single density (FM) sector not yet supported, ignore
			continue;
		}
		idx &= ~IDAM_FLAGS_MASK; // clear flags

		if (idx < 128) {
			// Invalid IDAM offset, ignore
			continue;
		}
		idx -= 128;
		if (idx >= dmkTrackLen) {
			// Invalid IDAM offset, ignore
			continue;
		}

		if (int(idx) <= lastIdam) {
			// Invalid IDAM offset:
			//   must be strictly bigger than previous, ignore
			continue;
		}

		output.addIdam(idx);
		lastIdam = idx;
	}
}

void DMKDiskImage::writeTrackImpl(byte track, byte side, const RawTrack& input)
{
	assert(side < 2);
	if ((singleSided && side) || (track >= numTracks)) {
		// no such side/track, ignore write
		// TODO a possible enhancement is to extend the file with
		//      extra tracks (or even convert from single sided to
		//      double sided)
		return;
	}

	seekTrack(track, side);

	// Write idam table.
	byte idamOut[2 * 64] = {}; // zero-initialize
	const vector<unsigned>& idamIn = input.getIdamBuffer();
	for (int i = 0; i < std::min<int>(64, int(idamIn.size())); ++i) {
		int t = (idamIn[i] + 128) | FLAG_MFM_SECTOR;
		idamOut[2 * i + 0] = t & 0xff;
		idamOut[2 * i + 1] = t >> 8;
	}
	file->write(idamOut, sizeof(idamOut));

	// Write raw track data.
	assert(input.getLength() == dmkTrackLen);
	file->write(input.getRawBuffer(), dmkTrackLen);
}


void DMKDiskImage::readSectorImpl(unsigned logicalSector, byte* buf)
{
	byte track, side, sector;
	logToPhys(logicalSector, track, side, sector);
	RawTrack rawTrack;
	readTrack(track, side, rawTrack);

	RawTrack::Sector sectorInfo;
	if (!rawTrack.decodeSector(sector, sectorInfo)) {
		throw NoSuchSectorException("Sector not found");
	}
	// TODO should we check sector size == 512?
	//      crc errors? correct track/head?
	rawTrack.readBlock(sectorInfo.dataIdx, 512, buf);
}

void DMKDiskImage::writeSectorImpl(unsigned logicalSector, const byte* buf)
{
	byte track, side, sector;
	logToPhys(logicalSector, track, side, sector);
	RawTrack rawTrack;
	readTrack(track, side, rawTrack);

	RawTrack::Sector sectorInfo;
	if (!rawTrack.decodeSector(sector, sectorInfo)) {
		throw NoSuchSectorException("Sector not found");
	}
	// TODO do checks? see readSectorImpl()
	rawTrack.writeBlock(sectorInfo.dataIdx, 512, buf);

	writeTrack(track, side, rawTrack);
}

unsigned DMKDiskImage::getNbSectorsImpl() const
{
	unsigned t = singleSided ? numTracks : (2 * numTracks);
	return t * const_cast<DMKDiskImage*>(this)->getSectorsPerTrack();
}

bool DMKDiskImage::isWriteProtectedImpl() const
{
	return writeProtected || file->isReadOnly();
}

Sha1Sum DMKDiskImage::getSha1Sum()
{
	return file->getSha1Sum();
}

void DMKDiskImage::detectGeometryFallback()
{
	// The implementation in Disk::detectGeometryFallback() uses
	// getNbSectors(), but for DMK images that doesn't work before we know
	// the geometry.

	// detectGeometryFallback() is for example used when the bootsector
	// could not be read. For a DMK image this can happen when that sector
	// has CRC errors or is missing or deleted.

	setSectorsPerTrack(9);
	setNbSides(singleSided ? 1 : 2);
}

} // namespace openmsx
