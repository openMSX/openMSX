/****************************************************************/
/* LZ77 data decompression					*/
/* Copyright (c) 1994 by XelaSoft				*/
/* version history:						*/
/*   version 0.9, start date: 11-27-1994			*/
/****************************************************************/

#ifndef XSADISKIMAGE_HH
#define XSADISKIMAGE_HH

#include "SectorBasedDisk.hh"
#include "MemBuffer.hh"

namespace openmsx {

class File;

class XSADiskImage final : public SectorBasedDisk
{
public:
	XSADiskImage(Filename& filename, File& file);

private:
	// SectorBasedDisk
	void readSectorImpl (size_t sector,       SectorBuffer& buf) override;
	void writeSectorImpl(size_t sector, const SectorBuffer& buf) override;
	bool isWriteProtectedImpl() const override;

	MemBuffer<SectorBuffer> data;
};

} // namespace openmsx

#endif
