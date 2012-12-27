// $Id$

/****************************************************************/
/* LZ77 data decompression					*/
/* Copyright (c) 1994 by XelaSoft				*/
/* version history:						*/
/*   version 0.9, start date: 11-27-1994			*/
/****************************************************************/

#ifndef XSADISKIMAGE_HH
#define XSADISKIMAGE_HH

#include "SectorBasedDisk.hh"
#include <vector>

namespace openmsx {

class File;

class XSADiskImage : public SectorBasedDisk
{
public:
	XSADiskImage(Filename& filename, File& file);

private:
	// SectorBasedDisk
	virtual void readSectorImpl(size_t sector, byte* buf);
	virtual void writeSectorImpl(size_t sector, const byte* buf);
	virtual bool isWriteProtectedImpl() const;

	std::vector<byte> data;
};

} // namespace openmsx

#endif
