// $Id$

#ifndef __DSKDISKIMAGE__HH__
#define __DSKDISKIMAGE__HH__

#include "SectorBasedDisk.hh"
#include "File.hh"

class FileContext;


class DSKDiskImage : public SectorBasedDisk
{
	public: 
		DSKDiskImage(FileContext &context,
		        const string &fileName);
		virtual ~DSKDiskImage();

		virtual void read(byte track, byte sector,
		                  byte side, int size, byte* buf);
		virtual void write(byte track, byte sector,
		                   byte side, int size, const byte* buf);

		virtual bool writeProtected();
		virtual bool doubleSided();

	protected:
		virtual void readBootSector();
		int nbSectors;

	private:
		File file;
};

#endif
