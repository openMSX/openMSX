// $Id$

#ifndef __FDC_DSK__HH__
#define __FDC_DSK__HH__

#include "FDCBackEnd.hh"

class File;
class FileContext;


const int SECTOR_SIZE = 512;
const int RAWTRACK_SIZE = 6850;

class FDC_DSK : public FDCBackEnd
{
	public: 
		FDC_DSK(FileContext *context,
		        const std::string &fileName);
		virtual ~FDC_DSK();
		virtual void read(byte track, byte sector,
		                  byte side, int size, byte* buf);
		virtual void write(byte track, byte sector,
		                   byte side, int size, const byte* buf);

		virtual void initWriteTrack(byte track, byte side);
		virtual void writeTrackData(byte data);

		virtual void initReadTrack(byte track, byte side);
		virtual byte readTrackData();

		virtual bool ready();
		virtual bool writeProtected();
		virtual bool doubleSided();


	protected:
		virtual void readBootSector();
		int nbSectors;

	private:
		File* file;
		byte* writeTrackBuf;
		int writeTrackBufCur;
		int writeTrackSectorCur;
		byte writeTrack_track;
		byte writeTrack_side;
		byte writeTrack_sector;
		int writeTrack_CRCcount;
		byte* readTrackDataBuf;
		int readTrackDataCount;
		int tmpdebugcounter;
};

#endif
