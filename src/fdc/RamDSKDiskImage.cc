// $Id$

#include "RamDSKDiskImage.hh"
#include "FileException.hh"
#include "CliComm.hh"
#include "StringOp.hh"

using std::string;

namespace openmsx {

RamDSKDiskImage::RamDSKDiskImage()
{
	RamDSKDiskImage(737280);
}

RamDSKDiskImage::RamDSKDiskImage(const int size)
{
	nbSectors = size / SECTOR_SIZE;
	diskdata = new byte[size];
	CliComm::instance().printInfo("RamDSk diskimage info");
	CliComm::instance().printInfo( StringOp::toString(size) );
	CliComm::instance().printInfo( StringOp::toString(nbSectors) );
}

RamDSKDiskImage::~RamDSKDiskImage()
{
	delete[] diskdata;
}

void RamDSKDiskImage::read(byte track, byte sector, byte side,
                           unsigned /*size*/, byte* buf)
{
	try {
		unsigned logicalSector = physToLog(track, side, sector);
		if (logicalSector >= nbSectors) {
			throw NoSuchSectorException("No such sector");
		}
		readLogicalSector(logicalSector, buf);
	} catch (MSXException& e) {
		throw DiskIOErrorException("Disk I/O error");
	}
}

void RamDSKDiskImage::readLogicalSector(unsigned sector, byte* buf)
{
	memcpy(buf,diskdata+sector*SECTOR_SIZE, SECTOR_SIZE);
}

void RamDSKDiskImage::write(byte track, byte sector, byte side, 
                         unsigned /*size*/, const byte* buf)
{
	if (writeProtected()) {
		throw WriteProtectedException("");
	}
	unsigned logicalSector = physToLog(track, side, sector);
	if (logicalSector >= nbSectors) {
		throw NoSuchSectorException("No such sector");
	}
	//file->write(buf, SECTOR_SIZE);
	memcpy(diskdata+logicalSector*SECTOR_SIZE,buf, SECTOR_SIZE);
}

bool RamDSKDiskImage::writeProtected()
{
	return false;
}

} // namespace openmsx
