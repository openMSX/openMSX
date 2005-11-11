// $Id$

#include "Disk.hh"

namespace openmsx {

Disk::Disk()
	: nbSides(0)
{
}

Disk::~Disk()
{
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

void Disk::initWriteTrack(byte /*track*/, byte /*side*/)
{
	PRT_DEBUG("Disk::initWriteTrack [unimplemented]");
}

void Disk::writeTrackData(byte /*data*/)
{
	PRT_DEBUG("Disk::writeTrackData [unimplemented]");
}

void Disk::initReadTrack(byte /*track*/, byte /*side*/)
{
	PRT_DEBUG("Disk::initReadTrack [unimplemented]");
}

byte Disk::readTrackData()
{
	PRT_DEBUG("Disk::readTrackData [unimplemented]");
	return 0xF4;
}

void Disk::applyPatch(const std::string& /*patchFile*/)
{
	throw MSXException("Patching of this disk image format not supported.");
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

void Disk::detectGeometryFallback() // if all else fails, use statistics
{
	// TODO maybe also check for 8*80*512 for 8 sectors per track
	sectorsPerTrack = 9; // most of the time (sorry 5.25" disk users...)
	// 360k disks are likely to be single sided:
	nbSides = (getImageSize() == (720 * 512)) ? 1 : 2;
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
		byte buf[512];
		read(0, 1, 0, 512, buf);
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
			read(0, 2, 0, 512, buf);
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
	} catch (MSXException& e) {
		// read error, assume it's a 3.5" DS or SS DD disk
		detectGeometryFallback();
	}
}

int Disk::getImageSize()
{
	return 0; // by default we know nothing of the image size
}

} // namespace openmsx

