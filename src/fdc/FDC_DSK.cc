// $Id$

#include "FDC_DSK.hh"


const int SECTOR_SIZE = 512;


FDC_DSK::FDC_DSK(const std::string &fileName)
{
	file = FileOpener::openFilePreferRW(fileName);
	file->seekg(0, std::ios::end);
	nbSectors = file->tellg() / SECTOR_SIZE;
}

FDC_DSK::~FDC_DSK()
{
	delete file;
}

void FDC_DSK::read(byte phystrack, byte track, byte sector, byte side,
                   int size, byte* buf)
{
	int logicalSector = physToLog(track, side, sector);
	if (logicalSector >= nbSectors)
		throw NoSuchSectorException("No such sector");
	file->seekg(logicalSector*SECTOR_SIZE, std::ios::beg);
	file->read(buf, SECTOR_SIZE);
	if (file->bad())
		throw DiskIOErrorException("Disk I/O error");
}

void FDC_DSK::write(byte phystrack, byte track, byte sector, byte side, 
                    int size, const byte* buf)
{
	int logicalSector = physToLog(track, side, sector);
	if (logicalSector >= nbSectors)
		throw NoSuchSectorException("No such sector");
	file->seekg(logicalSector*SECTOR_SIZE, std::ios::beg);
	file->write(buf, SECTOR_SIZE);
	if (file->bad())
		throw DiskIOErrorException("Disk I/O error");
}

void FDC_DSK::readBootSector()
{
	if (nbSectors == 1440) {
		sectorsPerTrack = 9;
		nbSides = 2;
	} else if (nbSectors == 720) {
		sectorsPerTrack = 9;
		nbSides = 1;
	} else {
		FDCBackEnd::readBootSector();
	}
}
