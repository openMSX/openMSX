// $Id$

#ifndef __FDC_DSK__HH__
#define __FDC_DSK__HH__

#include "FDCBackEnd.hh"
#include "File.hh"

const int SECTOR_SIZE = 512;

class FDC_DSK : public FDCBackEnd
{
	public: 
		FDC_DSK(const std::string &fileName);
		virtual ~FDC_DSK();
		virtual void read(byte phystrack, byte track, byte sector,
		                  byte side, int size, byte* buf);
		virtual void write(byte phystrack, byte track, byte sector,
		                   byte side, int size, const byte* buf);

		virtual void initWriteTrack(byte phystrack, byte track, byte side);
		virtual void writeTrackData(byte data);

	protected:
		virtual void readBootSector();
		int nbSectors;

	private:
		File* file;
		byte* writeTrackBuf;
		int writeTrackBufCur;
		int writeTrackSectorCur;
		byte writeTrack_phystrack;
		byte writeTrack_track;
		byte writeTrack_side;
		byte writeTrack_sector;
		int writeTrack_CRCcount;
		int tmpdebugcounter;
};

#endif
