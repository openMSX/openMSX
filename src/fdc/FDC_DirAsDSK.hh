// $Id$

#ifndef __FDC_DirAsDSK__HH__
#define __FDC_DirAsDSK__HH__


#include "FDC_CONSTS.hh"
#include "FDCBackEnd.hh"
#define MAX_CLUSTER 720

class FileContext;

struct MSXDirEntry {
	byte filename[8];
	byte ext[3];
	byte attrib;
	byte reserved[10]; /* unused */
	byte time[2];
	byte date[2];
	byte startcluster[2];
	byte size[4];
};

struct MappedDirEntry {
	MSXDirEntry msxinfo;
	std::string filename;
};

struct ReverseCluster {
	int dirEntryNr;
	long fileOffset;
};

class FDC_DirAsDSK : public FDCBackEnd
{
	public: 
		FDC_DirAsDSK(FileContext *context,
		        const string &fileName);
		virtual ~FDC_DirAsDSK();
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
		byte writeTrackBuf[SECTOR_SIZE];
		int writeTrackBufCur;
		int writeTrackSectorCur;
		byte writeTrack_track;
		byte writeTrack_side;
		byte writeTrack_sector;
		int writeTrack_CRCcount;
		byte readTrackDataBuf[RAWTRACK_SIZE];
		int readTrackDataCount;

		//Extentsion for fakedisk
		void updateFileInDSK(std::string fullfilename);
		bool checkFileUsedInDSK(std::string fullfilename);
		bool checkMSXFileExists(std::string fullfilename);
		std::string makeSimpleMSXFileName(std::string fullfilename);
		void addFileToDSK(std::string fullfilename);
		//int findFirstAvailableCluster(void);
		//int markClusterGetNext(void);
		word ReadFAT(word clnr);
		void WriteFAT(word clnr, word val);
		MappedDirEntry mapdir[112];	// max nr of entries in root directory: 7 sectors, each 16 entries
		ReverseCluster clustermap[MAX_CLUSTER];
		byte *FAT;

		static const byte BootBlock[];
};

#endif
