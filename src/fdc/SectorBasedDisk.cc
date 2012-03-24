// $Id$

#include "SectorBasedDisk.hh"
#include "DiskExceptions.hh"
#include "EmptyDiskPatch.hh"
#include "IPSPatch.hh"
#include <cassert>

namespace openmsx {

SectorBasedDisk::SectorBasedDisk(const DiskName& name)
	: Disk(name)
	, nbSectors(unsigned(-1)) // to detect misuse
{
}

SectorBasedDisk::~SectorBasedDisk()
{
}

void SectorBasedDisk::writeTrackImpl(byte track, byte side, const RawTrack& input)
{
	std::vector<RawTrack::Sector> sectors = input.decodeAll();
	for (std::vector<RawTrack::Sector>::const_iterator it = sectors.begin();
	     it != sectors.end(); ++it) {
		// Ignore 'track' and 'head' information
		// Always assume sectorsize = 512 (so also ignore sizeCode).
		// Ignore CRC value/errors of both address and data.
		// Ignore sector type (deleted or not)
		// Ignore sectors that are outside the range 1..sectorsPerTrack
		if ((it->sector < 1) || (it->sector > getSectorsPerTrack())) continue;
		byte buf[512];
		input.readBlock(it->dataIdx, 512, buf);
		unsigned logicalSector = physToLog(track, side, it->sector);
		writeSector(logicalSector, buf);
		// it's important to use writeSector() and not writeSectorImpl()
		// because only the former flushes SHA1 cache
	}
}

void SectorBasedDisk::readTrack(byte track, byte side, RawTrack& output)
{
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

	output.clear(); // clear idam positions

	int idx = 0;
	for (int i = 0; i < 80; ++i) output.write(idx++, 0x4E); // gap4a
	for (int i = 0; i < 12; ++i) output.write(idx++, 0x00); // sync
	for (int i = 0; i <  3; ++i) output.write(idx++, 0xC2); // index mark (1)
	for (int i = 0; i <  1; ++i) output.write(idx++, 0xFC); //            (2)
	for (int i = 0; i < 50; ++i) output.write(idx++, 0x4E); // gap1

	for (int j = 0; j < 9; ++j) {
		for (int i = 0; i < 12; ++i) output.write(idx++, 0x00); // sync

		for (int i = 0; i <  3; ++i) output.write(idx++, 0xA1); // addr mark (1)
		output.addIdam(idx);
		for (int i = 0; i <  1; ++i) output.write(idx++, 0xFE); //           (2)
		output.write(idx++, track); // C: Cylinder number
		output.write(idx++, side);  // H: Head Address
		output.write(idx++, j + 1); // R: Record
		output.write(idx++, 0x02);  // N: Number (length of sector: 512 = 128 << 2)
		word addrCrc = output.calcCrc(idx - 8, 8);
		output.write(idx++, addrCrc >> 8);   // CRC (high byte)
		output.write(idx++, addrCrc & 0xff); //     (low  byte)

		for (int i = 0; i < 22; ++i) output.write(idx++, 0x4E); // gap2
		for (int i = 0; i < 12; ++i) output.write(idx++, 0x00); // sync

		for (int i = 0; i <  3; ++i) output.write(idx++, 0xA1); // data mark (1)
		for (int i = 0; i <  1; ++i) output.write(idx++, 0xFB); //           (2)

		unsigned logicalSector = physToLog(track, side, j + 1);
		byte sectorBuf[512];
		readSector(logicalSector, sectorBuf);
		for (int i = 0; i < 512; ++i) output.write(idx++, sectorBuf[i]);

		word dataCrc = output.calcCrc(idx - (512 + 4), 512 + 4);
		output.write(idx++, dataCrc >> 8);   // CRC (high byte)
		output.write(idx++, dataCrc & 0xff); //     (low  byte)

		for (int i = 0; i < 84; ++i) output.write(idx++, 0x4E); // gap3
	}

	for (int i = 0; i < 182; ++i) output.write(idx++, 0x4E); // gap4b
	assert(idx == RawTrack::SIZE);
}

unsigned SectorBasedDisk::getNbSectorsImpl() const
{
	assert(nbSectors != unsigned(-1)); // must have been initialized
	return nbSectors;
}
void SectorBasedDisk::setNbSectors(unsigned num)
{
	assert(nbSectors == unsigned(-1)); // can only set this once
	nbSectors = num;
}

void SectorBasedDisk::detectGeometry()
{
	// the following are just heuristics...

	if (getNbSectors() == 1440) {
		// explicitly check for 720kb filesize

		// "trojka.dsk" is 720kb, but has bootsector and FAT media ID
		// for a single sided disk. From an emulator point of view it
		// must be accessed as a double sided disk.

		// "SDSNAT2.DSK" has invalid media ID in both FAT and
		// bootsector, other data in the bootsector is invalid as well.
		// Altough the first byte of the bootsector is 0xE9 to indicate
		// valid bootsector data. The only way to detect the format is
		// to look at the diskimage filesize.

		setSectorsPerTrack(9);
		setNbSides(2);

	} else {
		// Don't check for "360kb -> single sided disk". The MSXMania
		// disks are double sided disk but are truncated at 360kb.
		Disk::detectGeometry();
	}
}

} // namespace openmsx
