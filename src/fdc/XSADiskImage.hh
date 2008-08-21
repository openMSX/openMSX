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

namespace openmsx {

class File;

class XSADiskImage : public SectorBasedDisk
{
public:
	explicit XSADiskImage(const Filename& fileName);
	virtual ~XSADiskImage();

private:
	static const int MAXSTRLEN = 254;
	static const int TBLSIZE = 16;
	static const int MAXHUFCNT = 127;

	// SectorBasedDisk
	virtual void readSectorSBD(unsigned sector, byte* buf);
	virtual void writeSectorSBD(unsigned sector, const byte* buf);
	virtual bool isWriteProtectedImpl() const;

	bool isXSAImage(File& file);
	inline byte charin();
	inline void charout(byte ch);
	void chkheader();
	void unlz77();
	int rdstrlen();
	int rdstrpos();
	bool bitin();
	void inithufinfo();
	void mkhuftbl();

	struct HufNode {
		HufNode* child1;
		HufNode* child2;
		int weight;
	};

	byte* inbufpos;		// pos in input buffer
	byte* outbuf;		// the output buffer
	byte* outbufpos;	// pos in output buffer

	int updhufcnt;
	int cpdist[TBLSIZE + 1];
	int cpdbmask[TBLSIZE];
	int tblsizes[TBLSIZE];
	HufNode huftbl[2 * TBLSIZE - 1];

	byte bitflg;		// flag with the bits
	byte bitcnt;		// nb bits left

	static const int cpdext[TBLSIZE];	// Extra bits for distance codes
};

} // namespace openmsx

#endif
