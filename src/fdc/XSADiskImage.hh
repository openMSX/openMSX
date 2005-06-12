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
	XSADiskImage(const std::string& fileName);
	virtual ~XSADiskImage();

	virtual bool writeProtected();

private:
	static const int MAXSTRLEN = 254;
	static const int TBLSIZE = 16;
	static const int MAXHUFCNT = 127;

	virtual void readLogicalSector(unsigned sector, byte* buf);
	virtual void writeLogicalSector(unsigned sector, const byte* buf);

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
