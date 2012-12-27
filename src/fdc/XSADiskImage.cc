// $Id$

#include "XSADiskImage.hh"
#include "DiskExceptions.hh"
#include "File.hh"
#include <cstring>

using std::string;
using std::vector;

namespace openmsx {

class XSAExtractor
{
public:
	explicit XSAExtractor(File& file);
	void getData(vector<byte>& data);

private:
	static const int MAXSTRLEN = 254;
	static const int TBLSIZE = 16;
	static const int MAXHUFCNT = 127;

	inline byte charin();
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

	vector<byte> outBuf;	// the output buffer
	const byte* inBufPos;	// pos in input buffer
	const byte* inBufEnd;

	int updhufcnt;
	int cpdist[TBLSIZE + 1];
	int cpdbmask[TBLSIZE];
	int tblsizes[TBLSIZE];
	HufNode huftbl[2 * TBLSIZE - 1];

	byte bitflg;		// flag with the bits
	byte bitcnt;		// nb bits left

	static const int cpdext[TBLSIZE];	// Extra bits for distance codes
};


// XSADiskImage

XSADiskImage::XSADiskImage(Filename& filename, File& file)
	: SectorBasedDisk(filename)
{
	XSAExtractor extractor(file);
	extractor.getData(data);
	setNbSectors(unsigned(data.size()) / 512);
}

void XSADiskImage::readSectorImpl(unsigned sector, byte* buf)
{
	memcpy(buf, &data[sector * SECTOR_SIZE], SECTOR_SIZE);
}

void XSADiskImage::writeSectorImpl(unsigned /*sector*/, const byte* /*buf*/)
{
	throw WriteProtectedException("Write protected");
}

bool XSADiskImage::isWriteProtectedImpl() const
{
	return true;
}


// XSAExtractor

const int XSAExtractor::cpdext[TBLSIZE] = {
	  0,  0,  0,  0,  1,  2,  3,  4, 5,  6,  7,  8,  9, 10, 11, 12
};

XSAExtractor::XSAExtractor(File& file)
{
	size_t size;
	inBufPos = file.mmap(size);
	inBufEnd = inBufPos + size;

	if ((charin() != 'P') || (charin() != 'C') ||
	    (charin() != 'K') || (charin() != '\010')) {
		throw MSXException("Not an XSA image");
	}

	chkheader();
	inithufinfo();	// initialize the cpdist tables
	unlz77();
}

void XSAExtractor::getData(vector<byte>& data)
{
	// destroys internal outBuf, but that's ok
	swap(data, outBuf);
}

// Get the next character from the input buffer
byte XSAExtractor::charin()
{
	if (inBufPos >= inBufEnd) {
		throw MSXException("Corrupt XSA image: unexpected end of file");
	}
	return *inBufPos++;
}

// check fileheader
void XSAExtractor::chkheader()
{
	// read original length (little endian)
	unsigned outBufLen = 0;
	for (int i = 0, base = 1; i < 4; ++i, base <<= 8) {
		outBufLen += base * charin();
	}
	// is only used as an optimization, it will work correctly
	// even if this field was corrupt (intentionally or not)
	outBuf.reserve(outBufLen);

	// skip compressed length
	inBufPos += 4;

	// skip original filename
	while (charin()) /*empty*/;
}

// the actual decompression algorithm itself
void XSAExtractor::unlz77()
{
	bitcnt = 0; // no bits read yet

	while (true) {
		if (bitin()) {
			// 1-bit
			int strlen = rdstrlen();
			if (strlen == (MAXSTRLEN + 1)) {
				 return;
			}
			unsigned strpos = rdstrpos();
			if ((strpos == 0) || (strpos > outBuf.size())) {
				throw MSXException(
					"Corrupt XSA image: invalid offset");
			}
			while (strlen--) {
				outBuf.push_back(*(outBuf.end() - strpos));
			}
		} else {
			// 0-bit
			outBuf.push_back(charin());
		}
	}
}

// read string length
int XSAExtractor::rdstrlen()
{
	if (!bitin()) return 2;
	if (!bitin()) return 3;
	if (!bitin()) return 4;

	byte nrbits;
	for (nrbits = 2; (nrbits != 7) && bitin(); ++nrbits) {
		// nothing
	}

	int len = 1;
	while (nrbits--) {
		len = (len << 1) | (bitin() ? 1:0);
	}
	return (len + 1);
}

// read string pos
int XSAExtractor::rdstrpos()
{
	HufNode* hufpos = &huftbl[2 * TBLSIZE - 2];

	while (hufpos->child1) {
		if (bitin()) {
			hufpos = hufpos->child2;
		} else {
			hufpos = hufpos->child1;
		}
	}
	byte cpdindex = byte(hufpos-huftbl);
	++tblsizes[cpdindex];

	int strpos;
	if (cpdbmask[cpdindex] >= 256) {
		byte strposlsb = charin();
		byte strposmsb = 0;
		for (byte nrbits = cpdext[cpdindex]-8; nrbits--; strposmsb |= (bitin() ? 1:0)) {
			strposmsb <<= 1;
		}
		strpos = strposlsb + 256 * strposmsb;
	} else {
		strpos = 0;
		for (byte nrbits = cpdext[cpdindex]; nrbits--; strpos |= (bitin() ? 1:0)) {
			strpos <<= 1;
		}
	}
	if ((updhufcnt--) == 0) {
		mkhuftbl();	// make the huffman table
	}
	return strpos + cpdist[cpdindex];
}

// read a bit from the input file
bool XSAExtractor::bitin()
{
	if (bitcnt == 0) {
		bitflg = charin();	// read bitflg
		bitcnt = 8;		// 8 bits left
	}
	bool temp = bitflg & 1;
	--bitcnt;			// 1 bit less
	bitflg >>= 1;

	return temp;
}

// initialize the huffman info tables
void XSAExtractor::inithufinfo()
{
	int offs = 1;
	for (int i = 0; i != TBLSIZE; ++i) {
		cpdist[i] = offs;
		cpdbmask[i] = 1<<cpdext[i];
		offs += cpdbmask[i];
	}
	cpdist[TBLSIZE] = offs;

	for (int i = 0; i != TBLSIZE; ++i) {
		tblsizes[i] = 0;            // reset the table counters
		huftbl[i].child1 = nullptr; // mark the leave nodes
	}
	mkhuftbl();	// make the huffman table
}

// Make huffman coding info
void XSAExtractor::mkhuftbl()
{
	// Initialize the huffman tree
	HufNode* hufpos = huftbl;
	for (int i = 0; i != TBLSIZE; ++i) {
		(hufpos++)->weight = 1+(tblsizes[i] >>= 1);
	}
	for (int i = TBLSIZE; i != 2*TBLSIZE-1; ++i) {
		(hufpos++)->weight = -1;
	}
	// Place the nodes in the correct manner in the tree
	while (huftbl[2*TBLSIZE-2].weight == -1) {
		HufNode* l1pos;
		HufNode* l2pos;
		for (hufpos=huftbl; !(hufpos->weight); ++hufpos) {
			// nothing
		}
		l1pos = hufpos++;
		while (!(hufpos->weight)) {
			++hufpos;
		}
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
				} else if (tempw < l2pos->weight) {
					l2pos = hufpos;
				}
			}
			++hufpos;
		}
		hufpos->weight = l1pos->weight+l2pos->weight;
		(hufpos->child1 = l1pos)->weight = 0;
		(hufpos->child2 = l2pos)->weight = 0;
	}
	updhufcnt = MAXHUFCNT;
}

} // namespace openmsx
