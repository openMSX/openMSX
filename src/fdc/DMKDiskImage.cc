#include "DMKDiskImage.hh"
#include "RawTrack.hh"
#include "DiskExceptions.hh"
#include "File.hh"
#include "FilePool.hh"
#include "likely.hh"
#include "one_of.hh"
#include "ranges.hh"
#include "xrange.hh"
#include <cassert>

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
static_assert(sizeof(DmkHeader) == 16);

constexpr byte FLAG_SINGLE_SIDED = 0x10;
constexpr unsigned IDAM_FLAGS_MASK = 0xC000;
constexpr unsigned FLAG_MFM_SECTOR = 0x8000;


[[nodiscard]] static /*constexpr*/ bool isValidDmkHeader(const DmkHeader& header)
{
	if (header.writeProtected != one_of(0x00, 0xff)) {
		return false;
	}
	unsigned trackLen = header.trackLen[0] + 256 * header.trackLen[1];
	if (trackLen >= 0x4000) return false; // too large track length
	if (trackLen <= 128)    return false; // too small
	if (header.flags & ~0xd0) return false; // unknown flag set
	return ranges::all_of(header.reserved, [](auto& r) { return r == 0; }) &&
	       ranges::all_of(header.format,   [](auto& f) { return f == 0; });
}

DMKDiskImage::DMKDiskImage(Filename filename, std::shared_ptr<File> file_)
	: Disk(std::move(filename))
	, file(std::move(file_))
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
	for (auto i : xrange(64)) {
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
	if (singleSided && (side != 0)) {
		// second side on a single-side image, ignore write.
		// TODO possible enhancement:  convert to double sided
		return;
	}
	if (unlikely(numTracks <= track)) {
		extendImageToTrack(track);
	}
	doWriteTrack(track, side, input);
}

void DMKDiskImage::doWriteTrack(byte track, byte side, const RawTrack& input)
{
	seekTrack(track, side);

	// Write idam table.
	byte idamOut[2 * 64] = {}; // zero-initialize
	const auto& idamIn = input.getIdamBuffer();
	for (auto i : xrange(std::min(64, int(idamIn.size())))) {
		int t = (idamIn[i] + 128) | FLAG_MFM_SECTOR;
		idamOut[2 * i + 0] = t & 0xff;
		idamOut[2 * i + 1] = t >> 8;
	}
	file->write(idamOut, sizeof(idamOut));

	// Write raw track data.
	assert(input.getLength() == dmkTrackLen);
	file->write(input.getRawBuffer(), dmkTrackLen);
}

void DMKDiskImage::extendImageToTrack(byte track)
{
	// extend image with empty tracks
	RawTrack emptyTrack(dmkTrackLen);
	byte numSides = singleSided ? 1 : 2;
	while (numTracks <= track) {
		for (auto side : xrange(numSides)) {
			doWriteTrack(numTracks, side, emptyTrack);
		}
		++numTracks;
	}

	// update header
	file->seek(1); // position in header where numTracks is stored
	byte numTracksByte = numTracks;
	file->write(&numTracksByte, sizeof(numTracksByte));
}


void DMKDiskImage::readSectorImpl(size_t logicalSector, SectorBuffer& buf)
{
	auto [track, side, sector] = logToPhys(logicalSector);
	RawTrack rawTrack;
	readTrack(track, side, rawTrack);

	if (auto sectorInfo = rawTrack.decodeSector(sector)) {
		// TODO should we check sector size == 512?
		//      crc errors? correct track/head?
		rawTrack.readBlock(sectorInfo->dataIdx, buf.raw);
	} else {
		throw NoSuchSectorException("Sector not found");
	}
}

void DMKDiskImage::writeSectorImpl(size_t logicalSector, const SectorBuffer& buf)
{
	auto [track, side, sector] = logToPhys(logicalSector);
	RawTrack rawTrack;
	readTrack(track, side, rawTrack);

	if (auto sectorInfo = rawTrack.decodeSector(sector)) {
		// TODO do checks? see readSectorImpl()
		rawTrack.writeBlock(sectorInfo->dataIdx, buf.raw);
	} else {
		throw NoSuchSectorException("Sector not found");
	}

	writeTrack(track, side, rawTrack);
}

size_t DMKDiskImage::getNbSectorsImpl() const
{
	unsigned t = singleSided ? numTracks : (2 * numTracks);
	return t * const_cast<DMKDiskImage*>(this)->getSectorsPerTrack();
}

bool DMKDiskImage::isWriteProtectedImpl() const
{
	return writeProtected || file->isReadOnly();
}

Sha1Sum DMKDiskImage::getSha1SumImpl(FilePool& filepool)
{
	return filepool.getSha1Sum(*file);
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
