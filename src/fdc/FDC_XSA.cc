// $Id$

#include "FDC_XSA.hh"


const int FDC_XSA::cpdext[TBLSIZE] = {
	  0,  0,  0,  0,  1,  2,  3,  4, 5,  6,  7,  8,  9, 10, 11, 12
};


FDC_XSA::FDC_XSA(const FileContext *context, const std::string &fileName)
{
	File *file = new File(context, fileName);
	if (!isXSAImage(file)) {
		throw MSXException("Not an XSA image");
	}
	int fileSize = file->size();
	byte* inbuf = new byte[fileSize];
	inbufpos = inbuf;
	file->seek(0);
	file->read(inbuf, fileSize);
	delete file;
	
	chkheader();
	inithufinfo();	// initialize the cpdist tables
	unlz77();

	delete[] inbuf;
}

bool FDC_XSA::isXSAImage(File *file)
{
	byte buffer[4];
	file->read(buffer, 4);

	for (int i = 0; i < 4; i++) {
		if (buffer[i] != "PCK\010"[i]) {
			return false;
		}
	}
	return true;
}

FDC_XSA::~FDC_XSA()
{
	delete[] outbuf;
}

void FDC_XSA::read(byte track, byte sector,
                   byte side, int size, byte* buf)
{
	int logSector = physToLog(track, side, sector);
	if (logSector >= nbSectors)
		throw NoSuchSectorException("No such sector");
	memcpy(buf, outbuf + logSector * 512, 512);
}

void FDC_XSA::write(byte track, byte sector,
                    byte side, int size, const byte* buf)
{
	throw WriteProtectedException("Write protected");
}

void FDC_XSA::readBootSector()
{
	if (nbSectors == 1440) {
		sectorsPerTrack = 9;
		nbSides = 2;
	} else if (nbSectors == 720) {
		sectorsPerTrack = 9;
		nbSides = 1;
	} else {
		FDCBackEnd::readBootSector();
	}
}


// Get the next character from the input buffer
byte FDC_XSA::charin()
{
	return *(inbufpos++);
}

// Put the next character in the output buffer
void FDC_XSA::charout(byte ch)
{
	*(outbufpos++) = ch;
}


// check fileheader
void FDC_XSA::chkheader()
{
	// skip id
	inbufpos += 4;

	// read original length (low endian)
	int origLen = 0;
	for (int i=0, base = 1; i < 4; i++, base <<= 8) {
		origLen += base * charin();
	}
	nbSectors = origLen / 512;
	
	// skip compressed length
	inbufpos += 4;
	
	outbuf = new byte[origLen];
	outbufpos = outbuf;

	// skip original filename
	while (charin());
}

// the actual decompression algorithm itself
void FDC_XSA::unlz77()
{
	bitcnt = 0;	// no bits read yet
	
	while (true) {
		if (bitin()) {
			// 1-bit
			int strlen = rdstrlen();
			if (strlen == (MAXSTRLEN+1))
				 return;
			int strpos = rdstrpos();
			while (strlen--)
				 charout(*(outbufpos - strpos));
		} else {
			// 0-bit
			charout(charin());
		}
	}
}

// read string length
int FDC_XSA::rdstrlen()
{
	if (!bitin())
		return 2;
	if (!bitin())
		return 3;
	if (!bitin())
		return 4;

	byte nrbits;
	for (nrbits = 2; (nrbits != 7) && bitin(); nrbits++);

	int len = 1;
	while (nrbits--)
		len = (len << 1) | bitin();

	return (len + 1);
}

// read string pos
int FDC_XSA::rdstrpos()
{
	huf_node *hufpos = huftbl + 2*TBLSIZE - 2;

	while (hufpos->child1)
		if (bitin())
			hufpos = hufpos->child2;
		else
			hufpos = hufpos->child1;
	byte cpdindex = hufpos-huftbl;
	tblsizes[cpdindex]++;

	int strpos;
	if (cpdbmask[cpdindex] >= 256) {
		byte strposlsb = charin();
		byte strposmsb = 0;
		for (byte nrbits = cpdext[cpdindex]-8; nrbits--; strposmsb |= bitin())
			strposmsb <<= 1;
		strpos = strposlsb + 256 * strposmsb;
	} else {
		strpos = 0;
		for (byte nrbits = cpdext[cpdindex]; nrbits--; strpos |= bitin())
			strpos <<= 1;
	}    
	if ((updhufcnt--) == 0)
		mkhuftbl();	// make the huffman table

	return strpos + cpdist[cpdindex];
}

// read a bit from the input file
bool FDC_XSA::bitin()
{
	if (bitcnt == 0) {
		bitflg = charin();	// read bitflg
		bitcnt = 8;		// 8 bits left
	}
	bool temp = bitflg & 1;
	bitcnt--;			// 1 bit less
	bitflg >>= 1;

	return temp;
}

// initialize the huffman info tables
void FDC_XSA::inithufinfo()
{
	int offs = 1;
	for (int i = 0; i != TBLSIZE; i++) {
		cpdist[i] = offs;
		cpdbmask[i] = 1<<cpdext[i];
		offs += cpdbmask[i];
	}
	cpdist[TBLSIZE] = offs;

	for (int i = 0; i != TBLSIZE; i++) {
		tblsizes[i] = 0;	// reset the table counters
		huftbl[i].child1 = 0;	// mark the leave nodes
	}
	mkhuftbl();	// make the huffman table
}

// Make huffman coding info
void FDC_XSA::mkhuftbl()
{
	// Initialize the huffman tree
	huf_node *hufpos = huftbl;
	for (int i = 0; i != TBLSIZE; i++) {
		(hufpos++)->weight = 1+(tblsizes[i] >>= 1);
	}
	for (int i = TBLSIZE; i != 2*TBLSIZE-1; i++)
		(hufpos++)->weight = -1;

	// Place the nodes in the correct manner in the tree
	while (huftbl[2*TBLSIZE-2].weight == -1) {
		huf_node  *l1pos, *l2pos;
		for (hufpos=huftbl; !(hufpos->weight); hufpos++)
			;
		l1pos = hufpos++;
		while (!(hufpos->weight))
			hufpos++; 
		if (hufpos->weight < l1pos->weight) {
			l2pos = l1pos;
			l1pos = hufpos++;
		} else {
			l2pos = hufpos++;
		}
		int tempw;
		while ((tempw = (hufpos)->weight) != -1) {
			if (tempw) {
				if (tempw < l1pos->weight) {
					l2pos = l1pos;
					l1pos = hufpos;
				} else if (tempw < l2pos->weight)
					l2pos = hufpos;
			}
			hufpos++;
		}
		hufpos->weight = l1pos->weight+l2pos->weight;
		(hufpos->child1 = l1pos)->weight = 0;
		(hufpos->child2 = l2pos)->weight = 0;
	}
	updhufcnt = MAXHUFCNT;
}

bool FDC_XSA::ready()
{
	return true;
}

bool FDC_XSA::writeProtected()
{
	return true;
}

bool FDC_XSA::doubleSided()
{
	return nbSides == 2;
}
