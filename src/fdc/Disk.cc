#include "Disk.hh"
#include "DiskExceptions.hh"
#include "narrow.hh"
#include "one_of.hh"

namespace openmsx {

Disk::Disk(DiskName name_)
	: name(std::move(name_)), nbSides(0)
{
}

void Disk::writeTrack(uint8_t track, uint8_t side, const RawTrack& input)
{
	if (isWriteProtected()) {
		throw WriteProtectedException();
	}
	writeTrackImpl(track, side, input);
	flushCaches();
}

bool Disk::isDoubleSided()
{
	if (!nbSides) {
		detectGeometry();
	}
	return nbSides == 2;
}

// Note: Special case to convert between logical/physical sector numbers
//       for the boot sector and the 1st FAT sector (logical sector: 0/1,
//       physical location: track 0, side 0, sector 1/2): perform this
//       conversion without relying on the detected geometry parameters.
//       Otherwise the detectGeometry() method (which itself reads these
//       two sectors) would get in an infinite loop.
size_t Disk::physToLog(uint8_t track, uint8_t side, uint8_t sector)
{
	if ((track == 0) && (side == 0)) {
		return sector - 1;
	}
	if (!nbSides) {
		detectGeometry();
	}
	return sectorsPerTrack * (side + nbSides * track) + (sector - 1);
}
Disk::TSS Disk::logToPhys(size_t log)
{
	if (log <= 1) {
		uint8_t track = 0;
		uint8_t side = 0;
		auto sector = narrow<uint8_t>(log + 1);
		return {track, side, sector};
	}
	if (!nbSides) {
		detectGeometry();
	}
	auto track  = narrow<uint8_t>(log / (size_t(nbSides) * sectorsPerTrack));
	auto side   = narrow<uint8_t>((log / sectorsPerTrack) % nbSides);
	auto sector = narrow<uint8_t>((log % sectorsPerTrack) + 1);
	return {track, side, sector};
}

unsigned Disk::getSectorsPerTrack()
{
	if (!nbSides) {
		detectGeometry();
	}
	return sectorsPerTrack;
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
	//     DPB for MS-DOS, Version 2.0 and above. The DPB for MSX-DOS can
	//     be obtained as follows.
	//
	//     ....
	//     +18 +19  Sectors per track
	//     +1A +1B  Number of heads
	//     ...

	//
	//     Media Descriptor  0F8H  0F9H  0FAh  0FBH  0FCH  0FDH  0FEH  0FFH
	//       byte (FAT-ID)
	//     Sectors/track        9     9     8     8     9     9     8     8
	//     No. of sides         1     2     1     2     1     2     1     2
	//     Tracks/side         80    80    80    80    40    40    40    40
	//     ...

	try {
		SectorBuffer buf;
		readSector(0, buf); // boot sector
		if (buf.raw[0] == one_of(0xE9, 0xEB)) {
			// use values from boot sector
			sectorsPerTrack = buf.bootSector.sectorsTrack;
			nbSides         = buf.bootSector.nrSides;
			if ((sectorsPerTrack == 0) || (sectorsPerTrack > 255) ||
			    (nbSides         == 0) || (nbSides         > 255)) {
				// seems like bogus values, use defaults
				detectGeometryFallback();
			}
		} else {
			readSector(1, buf); // 1st fat sector
			auto mediaDescriptor = buf.raw[0];
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
