// $Id$

#ifndef __FDC_DirAsDSK__HH__
#define __FDC_DirAsDSK__HH__

#include "SectorBasedDisk.hh"

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
	string filename;
};

struct ReverseCluster {
	int dirEntryNr;
	long fileOffset;
};

class FDC_DirAsDSK : public SectorBasedDisk
{
	public: 
		FDC_DirAsDSK(FileContext &context,
		        const string &fileName);
		virtual ~FDC_DirAsDSK();
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
		static const int MAX_CLUSTER = 720;
		static const int SECTORS_PER_FAT = 3;

		void updateFileInDSK(const string& fullfilename);
		bool checkFileUsedInDSK(const string& fullfilename);
		bool checkMSXFileExists(const string& msxfilename);
		string makeSimpleMSXFileName(const string& fullfilename);
		void addFileToDSK(const string& fullfilename);
		//int findFirstAvailableCluster();
		//int markClusterGetNext();
		word ReadFAT(word clnr);
		void WriteFAT(word clnr, word val);
		MappedDirEntry mapdir[112];	// max nr of entries in root directory: 7 sectors, each 16 entries
		ReverseCluster clustermap[MAX_CLUSTER];
		byte FAT[SECTOR_SIZE * SECTORS_PER_FAT];

		static const byte BootBlock[];
};

#endif
