// $Id$

#include "Disk.hh"
#include "DiskExceptions.hh"

using std::string;

namespace openmsx {

Disk::Disk(const DiskName& name_)
	: name(name_), nbSides(0)
{
}

Disk::~Disk()
{
}

const DiskName& Disk::getName() const
{
	return name;
}

void Disk::write(byte track, byte sector,
                 byte side, unsigned size, const byte* buf)
{
	if (isWriteProtected()) {
		throw WriteProtectedException("");
	}
	writeImpl(track, sector, side, size, buf);
}

void Disk::writeTrackData(byte track, byte side, const byte* data)
{
	if (isWriteProtected()) {
		throw WriteProtectedException("");
	}
	writeTrackDataImpl(track, side, data);
}

void Disk::getTrackHeader(byte /*track*/, byte /*side*/, byte* /*buf*/)
{
	PRT_DEBUG("Disk::getTrackHeader [unimplemented]");
}
void Disk::getSectorHeader(byte /*track*/, byte /*sector*/, byte /*side*/,
                           byte* /*buf*/)
{
	PRT_DEBUG("Disk::getSectorHeader [unimplemented]");
}

void Disk::writeTrackDataImpl(byte /*track*/, byte /*side*/, const byte* /*data*/)
{
	PRT_DEBUG("Disk::writeTrackData [unimplemented]");
}

void Disk::readTrackData(byte /*track*/, byte /*side*/, byte* output)
{
	PRT_DEBUG("Disk::readTrackData [unimplemented]");
	for (int i = 0; i < RAWTRACK_SIZE; ++i) {
		output[i] = 0xF4;
	}
}

bool Disk::isDoubleSided() const
{
	return nbSides == 2;
}

int Disk::physToLog(byte track, byte side, byte sector)
{
	if ((track == 0) && (side == 0)) {
		// special case for bootsector or 1st FAT sector
		return sector - 1;
	}
	if (!nbSides) {
		detectGeometry();
	}
	int result = sectorsPerTrack * (side + nbSides * track) + (sector - 1);
	//PRT_DEBUG("Disk::physToLog(track " << (int)track << ", side "
	//          << (int)side << ", sector " << (int)sector<< ") returns "
	//          << result);
	return result;
}

void Disk::logToPhys(int log, byte& track, byte& side, byte& sector)
{
	if (!nbSides) {
		// try to guess values from boot sector
		detectGeometry();
	}
	track = log / (nbSides * sectorsPerTrack);
	side = (log / sectorsPerTrack) % nbSides;
	sector = (log % sectorsPerTrack) + 1;
}

void Disk::setSectorsPerTrack(unsigned num)
{
	sectorsPerTrack = num;
}
void Disk::setNbSides(unsigned num)
{
	nbSides = num;
}

void Disk::detectGeometryFallback() // if all else fails, use statistics
{
	// TODO maybe also check for 8*80 for 8 sectors per track
	sectorsPerTrack = 9; // most of the time (sorry 5.25" disk users...)
	// 360k disks are likely to be single sided:
	nbSides = (getNbSectors() == 720) ? 1 : 2;
}

void Disk::detectGeometry()
{
	// From the MSX Red Book (p265):
	//
	//  How to determine media types
	//
	//  a) Read the boot sector (track 0, sector 1) of the target drive
	//
	//  b) Check if the first byte is either 0E9H or 0EBH (the JMP
	//     instruction on 8086)
	//
	//  c) If step b) fails, the disk is a version prior to MS-DOS 2.0;
	//     therefore, use the first byte of the FAT passed from the caller
	//     and make sure it is between 0F8h and 0FFh.
	//
	//     If step c) is successful, use this as a media descriptor.
	//     If step c) fails, then this disk cannot be read.
	//
	//  d) If step b) succeeds, read bytes # 0B to # 1D. This is the
	//     DPB for MS-DOS, Version 2.0 and above. The DPB for MSXDOS can
	//     be obtained as follows.
	//
	//     ....
	//     +18 +19  Sectors per track
	//     +1A +1B  Number of heads
	//     ...

	//
	//     Media Descriptor  0F8H  0F9H  0FAh  0FBH  0FCH  0FDH  0FEH  0FFH
	//       byte (FATID)
	//     Sectors/track        9     9     8     8     9     9     8     8
	//     No. of sides         1     2     1     2     1     2     1     2
	//     Tracks/side         80    80    80    80    40    40    40    40
	//     ...

	try {
		byte buf[SECTOR_SIZE];
		read(0, 1, 0, SECTOR_SIZE, buf);
		if ((buf[0] == 0xE9) || (buf[0] ==0xEB)) {
			// use values from bootsector
			sectorsPerTrack = buf[0x18] + 256 * buf[0x19];
			nbSides         = buf[0x1A] + 256 * buf[0x1B];
			if ((sectorsPerTrack == 0) || (sectorsPerTrack > 255) ||
			    (nbSides         == 0) || (nbSides         > 255)) {
				// seems like bogus values, use defaults
				detectGeometryFallback();
			}
		} else {
			read(0, 2, 0, SECTOR_SIZE, buf);
			byte mediaDescriptor = buf[0];
			if (mediaDescriptor >= 0xF8) {
				sectorsPerTrack = (mediaDescriptor & 2) ? 8 : 9;
				nbSides         = (mediaDescriptor & 1) ? 2 : 1;
			} else {
				// invalid media descriptor, just assume it's a
				// normal DS or SS DD disk
				detectGeometryFallback();
			}
		}
	} catch (MSXException&) {
		// read error, assume it's a 3.5" DS or SS DD disk
		detectGeometryFallback();
	}
}

} // namespace openmsx

