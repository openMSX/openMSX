#include "SectorBasedDisk.hh"
#include "MSXException.hh"
#include "narrow.hh"
#include "xrange.hh"
#include <cassert>

namespace openmsx {

SectorBasedDisk::SectorBasedDisk(DiskName name_)
	: Disk(std::move(name_))
{
}

void SectorBasedDisk::writeTrackImpl(uint8_t track, uint8_t side, const RawTrack& input)
{
	for (auto& s : input.decodeAll()) {
		// Ignore 'track' and 'head' information
		// Always assume sector-size = 512 (so also ignore sizeCode).
		// Ignore CRC value/errors of both address and data.
		// Ignore sector type (deleted or not)
		// Ignore sectors that are outside the range 1..sectorsPerTrack
		if ((s.sector < 1) || (s.sector > getSectorsPerTrack())) continue;
		SectorBuffer buf;
		input.readBlock(s.dataIdx, buf.raw);
		auto logicalSector = physToLog(track, side, s.sector);
		writeSector(logicalSector, buf);
		// it's important to use writeSector() and not writeSectorImpl()
		// because only the former flushes SHA1 cache
	}
}

void SectorBasedDisk::readTrack(uint8_t track, uint8_t side, RawTrack& output)
{
	// Try to cache the last result of this method (the cache will be
	// flushed on any write to the disk). This very simple cache mechanism
	// will typically already have a very high hit-rate. For example during
	// emulation of a WD2793 read sector, we also emulate the search for
	// the correct sector. So the disk rotates from sector to sector, and
	// each time we re-read the track data (because EmuTime has passed).
	// Typically the software will also read several sectors from the same
	// track before moving to the next.
	checkCaches();
	int num = track | (side << 8);
	if (num == cachedTrackNum) {
		output = cachedTrackData;
		return;
	}
	cachedTrackNum = num;

	// This disk image only stores the actual sector data, not all the
	// extra gap, sync and header information that is in reality stored
	// in between the sectors. This function transforms the cooked sector
	// data back into raw track data. It assumes a standard IBM double
	// density, 9 sectors/track, 512 bytes/sector track layout.
	//
	// -- track --
	// gap4a         80 x 0x4e
	// sync          12 x 0x00
	// index mark     3 x 0xc2(*)
	//                1 x 0xfc
	// gap1          50 x 0x4e
	// 9 x [sector]   9 x [[658]]
	// gap4b        182 x 0x4e
	//
	// -- sector --
	// sync          12 x 0x00
	// ID addr mark   3 x 0xa1(*)
	//                1 x 0xfe
	// C H R N        4 x [..]
	// CRC            2 x [..]
	// gap2          22 x 0x4e
	// sync          12 x 0x00
	// data mark      3 x 0xa1(*)
	//                1 x 0xfb
	// data         512 x [..]    <-- actual sector data
	// CRC            2 x [..]
	// gap3          84 x 0x4e
	//
	// (*) Missing clock transitions in MFM encoding

	try {
		output.clear(RawTrack::STANDARD_SIZE); // clear idam positions

		int idx = 0;
		auto write = [&](unsigned n, uint8_t value) {
			repeat(n, [&] { output.write(idx++, value); });
		};

		write(80, 0x4E); // gap4a
		write(12, 0x00); // sync
		write( 3, 0xC2); // index mark (1)
		write( 1, 0xFC); //            (2)
		write(50, 0x4E); // gap1

		for (auto j : xrange(9)) {
			write(12, 0x00); // sync

			write( 3, 0xA1);                 // addr mark (1)
			output.write(idx++, 0xFE, true); //           (2) add idam
			output.write(idx++, track); // C: Cylinder number
			output.write(idx++, side);  // H: Head Address
			output.write(idx++, narrow<uint8_t>(j + 1)); // R: Record
			output.write(idx++, 0x02);  // N: Number (length of sector: 512 = 128 << 2)
			uint16_t addrCrc = output.calcCrc(idx - 8, 8);
			output.write(idx++, narrow_cast<uint8_t>(addrCrc >> 8));   // CRC (high byte)
			output.write(idx++, narrow_cast<uint8_t>(addrCrc & 0xff)); //     (low  byte)

			write(22, 0x4E); // gap2
			write(12, 0x00); // sync

			write( 3, 0xA1); // data mark (1)
			write( 1, 0xFB); //           (2)

			auto logicalSector = physToLog(track, side, narrow<uint8_t>(j + 1));
			SectorBuffer buf;
			readSector(logicalSector, buf);
			for (auto& r : buf.raw) output.write(idx++, r);

			uint16_t dataCrc = output.calcCrc(idx - (512 + 4), 512 + 4);
			output.write(idx++, narrow_cast<uint8_t>(dataCrc >> 8));   // CRC (high byte)
			output.write(idx++, narrow_cast<uint8_t>(dataCrc & 0xff)); //     (low  byte)

			write(84, 0x4E); // gap3
		}

		write(182, 0x4E); // gap4b
		assert(idx == RawTrack::STANDARD_SIZE);
	} catch (MSXException& /*e*/) {
		// There was an error while reading the actual sector data.
		// Most likely this is because we're reading the 81th track on
		// a disk with only 80 tracks (or similar). If you do this on a
		// real disk, you simply read an 'empty' track. So we do the
		// same here.
		output.clear(RawTrack::STANDARD_SIZE);
		cachedTrackNum = -1; // needed?
	}
	cachedTrackData = output;
}

void SectorBasedDisk::flushCaches()
{
	Disk::flushCaches();
	cachedTrackNum = -1;
}

size_t SectorBasedDisk::getNbSectorsImpl() const
{
	assert(nbSectors != size_t(-1)); // must have been initialized
	return nbSectors;
}
void SectorBasedDisk::setNbSectors(size_t num)
{
	assert(nbSectors == size_t(-1)); // can only set this once
	nbSectors = num;
}

void SectorBasedDisk::detectGeometry()
{
	// the following are just heuristics...

	if (getNbSectors() == 1440) {
		// explicitly check for 720kb filesize

		// "trojka.dsk" is 720kb, but has boot sector and FAT media ID
		// for a single sided disk. From an emulator point of view it
		// must be accessed as a double sided disk.

		// "SDSNAT2.DSK" has invalid media ID in both FAT and
		// boot sector, other data in the boot sector is invalid as well.
		// Although the first byte of the boot sector is 0xE9 to indicate
		// valid boot sector data. The only way to detect the format is
		// to look at the disk image filesize.

		setSectorsPerTrack(9);
		setNbSides(2);

	} else {
		// Don't check for "360kb -> single sided disk". The MSXMania
		// disks are double sided disk but are truncated at 360kb.
		Disk::detectGeometry();
	}
}

} // namespace openmsx
