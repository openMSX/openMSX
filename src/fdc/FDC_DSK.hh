// $Id$

#ifndef __FDC_DSK__HH__
#define __FDC_DSK__HH__

#include "FDCBackEnd.hh"
#include "FileOpener.hh"


class FDC_DSK : public FDCBackEnd
{
	public: 
		FDC_DSK(const std::string &fileName);
		virtual ~FDC_DSK();
		virtual void read(byte phystrack, byte track, byte sector,
		                  byte side, int size, byte* buf);
		virtual void write(byte phystrack, byte track, byte sector,
		                   byte side, int size, const byte* buf);

	protected:
		virtual void readBootSector();
		int nbSectors;

	private:
		IOFILETYPE* file;
};

#endif
