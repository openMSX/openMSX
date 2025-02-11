#include "D88DiskImage.hh"
#include "RawTrack.hh"
#include "DiskExceptions.hh"
#include "File.hh"
#include "FilePool.hh"
#include "narrow.hh"
#include "one_of.hh"
#include "ranges.hh"
#include "xrange.hh"
#include <array>
#include <cassert>

namespace openmsx {

static constexpr int32_t D88_TRACK_MAX = 164;
static constexpr uint8_t D88_DISK_TYPE_2DD = 0x10;
static constexpr uint8_t D88_DISK_TYPE_1DD = 0x40;

static constexpr uint8_t D88_STATUS_ID_CRC_ERROR = 0xA0;
static constexpr uint8_t D88_STATUS_DATA_CRC_ERROR = 0xB0;
static constexpr uint8_t D88_STATUS_NO_ADDRESS_MARK = 0xE0;
static constexpr uint8_t D88_STATUS_NO_DATA_MARK = 0xF0;

struct D88Header
{
  std::array<uint8_t, 17> diskName;
  std::array<uint8_t, 9> reserved;
  uint8_t writeProtected;
  uint8_t diskType;
  int32_t diskSize;
};

struct D88SectorHeader
{
	uint8_t c;
	uint8_t h;
	uint8_t r;
	uint8_t n;
	uint16_t sectorsPerTrack;
	uint8_t densityType;
	uint8_t deleteFlag;
	uint8_t status;
	std::array<uint8_t, 5> reserved;
	uint16_t sectorDataSize;
};

[[nodiscard]] static /*constexpr*/ bool isValidD88Header(const D88Header& header, const std::array<int32_t, 164>& trackOffsets)
{
	if (header.writeProtected != one_of(0x00, 0x10)) {
		return false;
	}
	if (header.diskType != one_of(D88_DISK_TYPE_2DD, D88_DISK_TYPE_1DD)) {
		return false;
	}

    // check track offset
    uint32_t prevTrackOffs = trackOffsets[0];
	if (prevTrackOffs != sizeof(D88Header) + 4 * D88_TRACK_MAX)
		return false;
    for(int32_t i=1; i<160; ++i)
    {
        auto trackOffs = trackOffsets[i];
        auto trackSize = trackOffs - prevTrackOffs;
        if (trackSize <= 128 || trackSize >= 0x4000)
            return false;
		prevTrackOffs = trackOffs;
    }
    for(int32_t i=160; i<D88_TRACK_MAX; ++i)
    {
        if (trackOffsets[i] != 0)
            return false;
    }

	return ranges::all_of(header.reserved, [](auto& r) { return r == 0; });
}

D88DiskImage::D88DiskImage(const Filename& filename, std::shared_ptr<File> file_)
    : Disk(DiskName(std::move(filename)))
    , file(std::move(file_))
	, trackData(8192)
	, cachedTrackNo(-1)
{
    D88Header header;
    file->seek(0);
    file->read(std::span{&header, 1});
    file->read(std::span{&trackOffsets, 1});
	if (!isValidD88Header(header, trackOffsets)) {
		throw MSXException("Not a D88 image");
	}

    diskSize = header.diskSize;
    singleSided = header.diskType == D88_DISK_TYPE_1DD;
    writeProtected = header.writeProtected == 0x10;

	// read track 0 first sector
	D88SectorHeader sectorHeader;
	file->seek(trackOffsets[0]);
	file->read(std::span{ &sectorHeader, 1 });
	sectorsPerTrack = sectorHeader.sectorsPerTrack;

	trackSectors.reserve(32);
}

bool D88DiskImage::readD88Track(uint8_t track, uint8_t side)
{
    int32_t track_ = singleSided ? track : track * 2 + side;

    if (track_ >= 160)
        return false;

	// check readed track.
	if (cachedTrackNo == track_)
		return true;
	cachedTrackNo = -1;

    auto trackOffs = trackOffsets[track_];
    auto nextTrackOffs = trackOffsets[track_ + 1];
    if (nextTrackOffs < 1)
        nextTrackOffs = diskSize;
    auto trackSize = nextTrackOffs - trackOffs;

    if (trackOffs==0 || trackSize<1)
        return false;

	if (trackData.size() < trackSize)
	{
		trackData.resize(trackSize);
	}
	trackSectors.clear();

	/* Read TrackData */
    file->seek(trackOffs);
	file->read(std::span{ trackData });

    uint16_t inTrackOffset = 0;
    while(inTrackOffset < trackSize)
    {
		D88SectorHeader* pHeader = (D88SectorHeader*)(trackData.data() + inTrackOffset);
		D88SectorData sector;

		int nn = pHeader->n & 3;
		sector.c = pHeader->c;
		sector.h = pHeader->h;
		sector.r = pHeader->r;
		sector.n = pHeader->n;
		sector.deleteFlag = pHeader->deleteFlag;
		sector.status = pHeader->status;
		sector.sectorSize = 128 << nn;
		sector.dataOffset = inTrackOffset + 16;
		sector.dataSize = pHeader->sectorDataSize;

		inTrackOffset += (16 + pHeader->sectorDataSize);
		trackSectors.push_back(sector);
    }

	cachedTrackNo = track_;
    return true;
}

// Disk
void D88DiskImage::readTrack(uint8_t track, uint8_t side, RawTrack& output)
{
	assert(side < 2);
	if (singleSided && side) {
		// NoExists Side
		output.clear(RawTrack::STANDARD_SIZE);
        return;
	}

	if (!readD88Track(track, side))
	{
		// Track Not Found
		output.clear(RawTrack::STANDARD_SIZE);
        return;
	}

	// # preamble (146)
	//
	// GAP4a   0x4E :  80
	// SYNC    0x00 :  12
	// IAM     0xC2 :   3
	//         0xFC :   1
	// GAP1    0x4E :  50
	//
	// # sector (62 + datasize + gap3)
	//
	// SYNC    0x00 :  12
	// IDAM    0xA1 :   3
	//         0xFE :   1
	// CHRN      xx :   4
	// CRC       xx :   2
	// GAP2    0x4E :  22
	//
	// SYNC    0x00 :  12
	// DAM     0xA1 :   3
	//         0xFB :   1 (DDAM:0xF8)
	// data      xx : 128 << N (512)
	// CRC       xx :   2
	// GAP3    0x4E :  84 <- 512 byte 9 sec
	//
	// # postamble
	// GAP4b   0x4E : 182 <- 512 byte 9 sec
	int trackLen = 146;
	for(const auto& sct: trackSectors)
	{
		trackLen += 62 + 84 + sct.sectorSize;
	}
	trackLen += 182;
	output.clear(trackLen);

	int idx = 0;
	auto write = [&](unsigned n, uint8_t value) {
		repeat(n, [&] { output.write(idx++, value); });
	};
	auto trackDataSpan = std::span{trackData};

	// preamble
	write(80, 0x4E); // gap4a
	write(12, 0x00); // sync
	write( 3, 0xC2); // index mark (1)
	write( 1, 0xFC); //            (2)
	write(50, 0x4E); // gap1

	// sectors
	for(const auto& sct: trackSectors)
	{
		if (sct.status == D88_STATUS_NO_ADDRESS_MARK)
		{
			continue;
		}

		write(12, 0x00); // sync

		write( 3, 0xA1);                 	// addr mark (1)
		output.write(idx++, 0xFE, true); 	//           (2) add idam
		output.write(idx++, sct.c); 		// C: Cylinder number
		output.write(idx++, sct.h);  		// H: Head Address
		output.write(idx++, sct.r); 		// R: Record
		output.write(idx++, sct.n);  		// N: Number (length of sector: 512 = 128 << 2)
		uint16_t addrCrc = output.calcCrc(idx - 8, 8);
		if (sct.status == D88_STATUS_ID_CRC_ERROR)
		{
			addrCrc = ~addrCrc;
		}
		output.write(idx++, narrow_cast<uint8_t>(addrCrc >> 8));   // CRC (high byte)
		output.write(idx++, narrow_cast<uint8_t>(addrCrc & 0xff)); //     (low  byte)
		
		write(22, 0x4E); // gap2

		if (sct.status == D88_STATUS_NO_DATA_MARK)
		{
			continue;
		}

		write(12, 0x00); // sync
		write( 3, 0xA1); // data mark (1)
		if (sct.deleteFlag == 0)
		{
			write( 1, 0xFB); //           (2)
		}
		else
		{
			write( 1, 0xF8); //           (2)
		}
		output.writeBlock(idx, trackDataSpan.subspan(sct.dataOffset, sct.dataSize));
		idx += sct.sectorSize;

		uint16_t dataCrc = output.calcCrc(idx - (sct.sectorSize + 4), sct.sectorSize + 4);
		if (sct.status == D88_STATUS_DATA_CRC_ERROR)
		{
			dataCrc = ~dataCrc;
		}
		output.write(idx++, narrow_cast<uint8_t>(dataCrc >> 8));   // CRC (high byte)
		output.write(idx++, narrow_cast<uint8_t>(dataCrc & 0xff)); //     (low  byte)

		write(84, 0x4E); // gap3
	}
	//write(182, 0x4E); // gap4b
}

void D88DiskImage::writeTrackImpl(uint8_t track, uint8_t side, const RawTrack& input)
{
	throw WriteProtectedException("Not Supported.");
	// assert(side < 2);
	// if (singleSided && (side != 0)) {
	// 	return;
	// }
	// if (numTracks <= track) [[unlikely]] {
	// 	extendImageToTrack(track);
	// }
	// doWriteTrack(track, side, input);
}

// SectorAccessibleDisk
void D88DiskImage::readSectorImpl(size_t logicalSector, SectorBuffer& buf)
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

void D88DiskImage::writeSectorImpl(size_t logicalSector, const SectorBuffer& buf)
{
	throw WriteProtectedException("Not Supported.");
	// auto [track, side, sector] = logToPhys(logicalSector);
	// RawTrack rawTrack;
	// readTrack(track, side, rawTrack);

	// if (auto sectorInfo = rawTrack.decodeSector(sector)) {
	// 	// TODO do checks? see readSectorImpl()
	// 	rawTrack.writeBlock(sectorInfo->dataIdx, buf.raw);
	// } else {
	// 	throw NoSuchSectorException("Sector not found");
	// }

	// writeTrack(track, side, rawTrack);
}

size_t D88DiskImage::getNbSectorsImpl() const
{
	size_t t = singleSided ? 80 : (2 * 80);
	return t * (sectorsPerTrack == 8 ? 8 : 9);
}

bool D88DiskImage::isWriteProtectedImpl() const
{
	return writeProtected || file->isReadOnly();
}

Sha1Sum D88DiskImage::getSha1SumImpl(FilePool& filePool)
{
	return filePool.getSha1Sum(*file);
}

void D88DiskImage::detectGeometryFallback()
{
	setSectorsPerTrack(9);
	setNbSides(singleSided ? 1 : 2);
}


}