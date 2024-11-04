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

class XSADiskImage final : public SectorBasedDisk
{
public:
	XSADiskImage(const Filename& filename, File& file);

private:
	// SectorBasedDisk
	void readSectorsImpl(
		std::span<SectorBuffer> buffers, size_t startSector) override;
	void writeSectorImpl(size_t sector, const SectorBuffer& buf) override;
	[[nodiscard]] bool isWriteProtectedImpl() const override;

	std::vector<SectorBuffer> data;
};

} // namespace openmsx

#endif
