// $Id$

/****************************************************************/
/* LZ77 data decompression					*/
/* Copyright (c) 1994 by XelaSoft				*/
/* version history:						*/
/*   version 0.9, start date: 11-27-1994			*/
/****************************************************************/

#ifndef __FDC_XSA_HH__
#define __FDC_XSA_HH__

#include "SectorBasedDisk.hh"

namespace openmsx {

class File;

class XSADiskImage : public SectorBasedDisk
{
public:
	XSADiskImage(const string& fileName);
	virtual ~XSADiskImage();
	
	virtual void read(byte track, byte sector,
			  byte side, int size, byte* buf);
	virtual void write(byte track, byte sector,
			   byte side, int size, const byte* buf);

	virtual bool writeProtected();
	virtual bool doubleSided();

private:
	static const int MAXSTRLEN = 254;
	static const int TBLSIZE = 16;
	static const int MAXHUFCNT = 127;

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

	typedef struct huf_node {
		int weight;
		huf_node* child1;
		huf_node* child2;
	} huf_node;

	byte* inbufpos;		// pos in input buffer
	byte* outbuf;		// the output buffer
	byte* outbufpos;	// pos in output buffer

	byte bitflg;		// flag with the bits
	byte bitcnt;		// nb bits left

	int updhufcnt;
	int cpdist[TBLSIZE + 1];
	int cpdbmask[TBLSIZE];
	int tblsizes[TBLSIZE];
	static const int cpdext[TBLSIZE];	// Extra bits for distance codes
	huf_node huftbl[2 * TBLSIZE - 1];
};

} // namespace openmsx

#endif
