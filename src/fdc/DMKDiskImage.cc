#include "DMKDiskImage.hh"

#include "DiskExceptions.hh"
#include "File.hh"
#include "FilePool.hh"
#include "RawTrack.hh"

#include "narrow.hh"
#include "one_of.hh"
#include "xrange.hh"

#include <algorithm>
#include <array>
#include <cassert>

namespace openmsx {

struct DmkHeader
{
	uint8_t writeProtected;
	uint8_t numTracks;
	std::array<uint8_t, 2> trackLen;
	uint8_t flags;
	std::array<uint8_t, 7> reserved;
	std::array<uint8_t, 4> format;
};
static_assert(sizeof(DmkHeader) == 16);

static constexpr uint8_t FLAG_SINGLE_SIDED = 0x10;
static constexpr unsigned IDAM_FLAGS_MASK = 0xC000;
static constexpr unsigned FLAG_MFM_SECTOR = 0x8000;


[[nodiscard]] static constexpr bool isValidDmkHeader(const DmkHeader& header)
{
	if (header.writeProtected != one_of(0x00, 0xff)) {
		return false;
	}
	unsigned trackLen = header.trackLen[0] + 256 * header.trackLen[1];
	if (trackLen >= 0x4000) return false; // too large track length
	if (trackLen <= 128)    return false; // too small
	if (header.flags & ~0xd0) return false; // unknown flag set
	return std::ranges::all_of(header.reserved, [](auto& r) { return r == 0; }) &&
	       std::ranges::all_of(header.format,   [](auto& f) { return f == 0; });
}

DMKDiskImage::DMKDiskImage(Filename filename, std::shared_ptr<File> file_)
	: Disk(DiskName(std::move(filename)))
	, file(std::move(file_))
{
	DmkHeader header;
	file->seek(0);
	file->read(std::span{&header, 1});
	if (!isValidDmkHeader(header)) {
		throw MSXException("Not a DMK image");
	}

	numTracks = header.numTracks;
	dmkTrackLen = header.trackLen[0] + 256 * header.trackLen[1] - 128;
	singleSided = (header.flags & FLAG_SINGLE_SIDED) != 0;
	writeProtected = header.writeProtected == 0xff;

	// TODO should we print a warning when dmkTrackLen is too far from the
	//      ideal value RawTrack::SIZE? This might indicate the disk image
	//      was not a 3.5" DD disk image and data will be lost on either
	//      read or write.
}

void DMKDiskImage::seekTrack(uint8_t track, uint8_t side)
{
	size_t t = singleSided ? track : (2 * track + side);
	file->seek(sizeof(DmkHeader) + t * (dmkTrackLen + 128));
}

void DMKDiskImage::readTrack(uint8_t track, uint8_t side, RawTrack& output)
{
	assert(side < 2);
	output.clear(dmkTrackLen);
	if ((singleSided && side) || (track >= numTracks)) {
		// no such side/track, only clear output
		return;
	}

	seekTrack(track, side);

	// Read idam data (still needs to be converted).
	std::array<uint8_t, 2 * 64> idamBuf;
	file->read(idamBuf);

	// Read raw track data.
	file->read(output.getRawBuffer());

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
		lastIdam = narrow<int>(idx);
	}
}

void DMKDiskImage::writeTrackImpl(uint8_t track, uint8_t side, const RawTrack& input)
{
	assert(side < 2);
	if (singleSided && (side != 0)) {
		// second side on a single-side image, ignore write.
		// TODO possible enhancement:  convert to double sided
		return;
	}
	if (numTracks <= track) [[unlikely]] {
		extendImageToTrack(track);
	}
	doWriteTrack(track, side, input);
}

void DMKDiskImage::doWriteTrack(uint8_t track, uint8_t side, const RawTrack& input)
{
	seekTrack(track, side);

	// Write idam table.
	std::array<uint8_t, 2 * 64> idamOut = {}; // zero-initialize
	for (const auto& idamIn = input.getIdamBuffer();
	     auto i : xrange(std::min(64, int(idamIn.size())))) {
		auto t = (idamIn[i] + 128) | FLAG_MFM_SECTOR;
		idamOut[2 * i + 0] = narrow<uint8_t>(t & 0xff);
		idamOut[2 * i + 1] = narrow<uint8_t>(t >> 8);
	}
	file->write(idamOut);

	// Write raw track data.
	assert(input.getLength() == dmkTrackLen);
	file->write(input.getRawBuffer());
}

void DMKDiskImage::extendImageToTrack(uint8_t track)
{
	// extend image with empty tracks
	RawTrack emptyTrack(dmkTrackLen);
	uint8_t numSides = singleSided ? 1 : 2;
	while (numTracks <= track) {
		for (auto side : xrange(numSides)) {
			doWriteTrack(numTracks, side, emptyTrack);
		}
		++numTracks;
	}

	// update header
	file->seek(1); // position in header where numTracks is stored
	std::array<uint8_t, 1> numTracksBuf = {numTracks};
	file->write(numTracksBuf);
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

size_t DMKDiskImage::getNbSectorsImpl()
{
	size_t t = singleSided ? numTracks : (2 * numTracks);
	return t * getSectorsPerTrack();
}

bool DMKDiskImage::isWriteProtectedImpl() const
{
	return writeProtected || file->isReadOnly();
}

Sha1Sum DMKDiskImage::getSha1SumImpl(FilePool& filePool)
{
	return filePool.getSha1Sum(*file, getName().getResolved());
}

void DMKDiskImage::detectGeometryFallback()
{
	// The implementation in Disk::detectGeometryFallback() uses
	// getNbSectors(), but for DMK images that doesn't work before we know
	// the geometry.

	// detectGeometryFallback() is for example used when the boot sector
	// could not be read. For a DMK image this can happen when that sector
	// has CRC errors or is missing or deleted.

	setSectorsPerTrack(9);
	setNbSides(singleSided ? 1 : 2);
}

} // namespace openmsx
