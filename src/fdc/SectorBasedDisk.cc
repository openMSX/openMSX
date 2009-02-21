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

void SectorBasedDisk::read(byte track, byte sector, byte side,
                           unsigned size, byte* buf)
{
	(void)size;
	assert(size == SECTOR_SIZE);
	unsigned logicalSector = physToLog(track, side, sector);
	readSector(logicalSector, buf);
}

void SectorBasedDisk::writeImpl(byte track, byte sector, byte side,
                                unsigned size, const byte* buf)
{
	(void)size;
	assert(size == SECTOR_SIZE);
	unsigned logicalSector = physToLog(track, side, sector);
	writeSector(logicalSector, buf);
	// it's important to use writeSector() and not writeSectorImpl()
	// because only the former flushes SHA1 cache
}

void SectorBasedDisk::writeTrackDataImpl(byte track, byte side, const byte* data)
{
	unsigned sector = 1;
	bool hasFirstCRC = false;
	for (int i = 0; i < RAWTRACK_SIZE; ++i) {
		// Look for two 0xF7 bytes (two CRC characters)
		// the first is the CRC for the sector header, the second is
		// for the actual sector data.
		// TODO this seems WD2793 specific, we might have to fix this
		//      once we implement more FDC write-raw-track commands
		if (data[i] == 0xF7) {
			if (!hasFirstCRC) {
				hasFirstCRC = true;
				// ... still wait for 2nd CRC byte ...
			} else {
				// ... found it, previous 512 bytes are sector data
				hasFirstCRC = false;
				if (i >= 512) {
					write(track, sector, side, 512,
					      &data[i - 512]);
				}
				++sector;
			}
		}
	}
}

void SectorBasedDisk::readTrackData(byte track, byte side, byte* output)
{
	// init following data structure
	// according to Alex Wulms
	// 122 bytes track header aka pre-gap
	// 9 * 628 bytes sectordata (sector header, data en closure gap)
	// 1080 bytes end-of-track gap
	//
	// This data comes from the TC8566AF manual
	// each track in IBM format contains
	//   '4E' x 80, '00' x 12, 'C2' x 3
	//   'FC' x  1, '4E' x 50
	//   sector data 1 to n
	//   '4E' x ?? (closing gap)
	// each sector data contains
	//   '00' x 12, 'A1' x 3, 'FE' x 1,
	//   C,H,R,N,CRC(2bytes), '4E' x 22, '00' x 12,
	//   'A1' x  4,'FB'('F8') x 1, data(512 bytes),CRC(2bytes),'4E'(gap3)

	byte* out = output;

	// track header
	for (int i = 0; i < 80; ++i) *out++ = 0x4E;
	for (int i = 0; i < 12; ++i) *out++ = 0x00;
	for (int i = 0; i <  3; ++i) *out++ = 0xC2;
	for (int i = 0; i <  1; ++i) *out++ = 0xFC;
	for (int i = 0; i < 50; ++i) *out++ = 0x4E;
	assert((out - output) == 146); // correct length?

	// sectors
	for (int j = 0; j < 9; ++j) {
		// sector header
		for (int i = 0; i < 12; ++i) *out++ = 0x00;
		for (int i = 0; i <  3; ++i) *out++ = 0xA1;
		for (int i = 0; i <  1; ++i) *out++ = 0xFE;
		*out++ = track; // C: Cylinder number
		*out++ = side;  // H: Head Address
		*out++ = j + 1; // R: Record
		*out++ = 0x02;  // N: Number (length of sector)
		*out++ = 0x00;  // CRC byte 1   TODO
		*out++ = 0x00;  // CRC byte 2
		for (int i = 0; i < 22; ++i) *out++ = 0x4E;
		for (int i = 0; i < 12; ++i) *out++ = 0x00;
		// sector data
		read(track, j + 1, side, 512, out);
		out += 512;
		*out++ = 0x00;  // CRC byte 1   TODO
		*out++ = 0x00;  // CRC byte 2
		// end-of-sector gap
		for (int i = 0; i < 58; ++i) *out++ = 0x4E;
	}
	assert((out - output) == (146 + 9 * 628)); // correct length?

	// end-of-track gap
	for (int i = 0; i < 1052; ++i) *out++ = 0x4E;
	assert((out - output) == RAWTRACK_SIZE);
}

bool SectorBasedDisk::isReady() const
{
	return true;
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
