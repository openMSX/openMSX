#include "XSADiskImage.hh"
#include "DiskExceptions.hh"
#include "File.hh"
#include <cstring>
#include <utility>

using std::string;

namespace openmsx {

class XSAExtractor
{
public:
	explicit XSAExtractor(File& file);
	std::pair<MemBuffer<SectorBuffer>, unsigned> extractData();

private:
	static constexpr int MAXSTRLEN = 254;
	static constexpr int TBLSIZE = 16;
	static constexpr int MAXHUFCNT = 127;

	inline byte charIn();
	void chkHeader();
	void unLz77();
	unsigned rdStrLen();
	int rdStrPos();
	bool bitIn();
	void initHufInfo();
	void mkHufTbl();

	struct HufNode {
		HufNode* child1;
		HufNode* child2;
		int weight;
	};

	MemBuffer<SectorBuffer> outBuf;	// the output buffer
	const byte* inBufPos;	// pos in input buffer
	const byte* inBufEnd;
	unsigned sectors;

	int updHufCnt;
	int cpDist[TBLSIZE + 1];
	int cpdBmask[TBLSIZE];
	int tblSizes[TBLSIZE];
	HufNode hufTbl[2 * TBLSIZE - 1];

	byte bitFlg;		// flag with the bits
	byte bitCnt;		// nb bits left

	static constexpr int cpdExt[TBLSIZE] = { // Extra bits for distance codes
		  0,  0,  0,  0,  1,  2,  3,  4, 5,  6,  7,  8,  9, 10, 11, 12
	};
};


// XSADiskImage

XSADiskImage::XSADiskImage(Filename& filename, File& file)
	: SectorBasedDisk(filename)
{
	XSAExtractor extractor(file);
	unsigned sectors;
	std::tie(data, sectors) = extractor.extractData();
	setNbSectors(sectors);
}

void XSADiskImage::readSectorImpl(size_t sector, SectorBuffer& buf)
{
	memcpy(&buf, &data[sector], sizeof(buf));
}

void XSADiskImage::writeSectorImpl(size_t /*sector*/, const SectorBuffer& /*buf*/)
{
	throw WriteProtectedException("Write protected");
}

bool XSADiskImage::isWriteProtectedImpl() const
{
	return true;
}


// XSAExtractor

XSAExtractor::XSAExtractor(File& file)
{
	auto mmap = file.mmap();
	inBufPos = mmap.begin();
	inBufEnd = mmap.end();

	if ((charIn() != 'P') || (charIn() != 'C') ||
	    (charIn() != 'K') || (charIn() != '\010')) {
		throw MSXException("Not an XSA image");
	}

	chkHeader();
	initHufInfo();	// initialize the cpDist tables
	unLz77();
}

std::pair<MemBuffer<SectorBuffer>, unsigned> XSAExtractor::extractData()
{
	// destroys internal outBuf, but that's ok
	return {std::move(outBuf), sectors};
}

// Get the next character from the input buffer
byte XSAExtractor::charIn()
{
	if (inBufPos >= inBufEnd) {
		throw MSXException("Corrupt XSA image: unexpected end of file");
	}
	return *inBufPos++;
}

// check fileheader
void XSAExtractor::chkHeader()
{
	// read original length (little endian)
	unsigned outBufLen = 0;
	for (int i = 0, base = 1; i < 4; ++i, base <<= 8) {
		outBufLen += base * charIn();
	}
	sectors = (outBufLen + 511) / 512;
	outBuf.resize(sectors);

	// skip compressed length
	inBufPos += 4;

	// skip original filename
	while (charIn()) /*empty*/;
}

// the actual decompression algorithm itself
void XSAExtractor::unLz77()
{
	bitCnt = 0; // no bits read yet

	size_t remaining = sectors * sizeof(SectorBuffer);
	byte* out = outBuf.data()->raw;
	size_t outIdx = 0;
	while (true) {
		if (bitIn()) {
			// 1-bit
			unsigned strLen = rdStrLen();
			if (strLen == (MAXSTRLEN + 1)) {
				 return;
			}
			unsigned strPos = rdStrPos();
			if ((strPos == 0) || (strPos > outIdx)) {
				throw MSXException(
					"Corrupt XSA image: invalid offset");
			}
			if (remaining < strLen) {
				throw MSXException(
					"Invalid XSA image: too small output buffer");
			}
			remaining -= strLen;
			while (strLen--) {
				out[outIdx] = out[outIdx - strPos];
				++outIdx;
			}
		} else {
			// 0-bit
			if (remaining == 0) {
				throw MSXException(
					"Invalid XSA image: too small output buffer");
			}
			--remaining;
			out[outIdx++] = charIn();
		}
	}
}

// read string length
unsigned XSAExtractor::rdStrLen()
{
	if (!bitIn()) return 2;
	if (!bitIn()) return 3;
	if (!bitIn()) return 4;

	byte nrBits;
	for (nrBits = 2; (nrBits != 7) && bitIn(); ++nrBits) {
		// nothing
	}

	unsigned len = 1;
	while (nrBits--) {
		len = (len << 1) | (bitIn() ? 1 : 0);
	}
	return (len + 1);
}

// read string pos
int XSAExtractor::rdStrPos()
{
	HufNode* hufPos = &hufTbl[2 * TBLSIZE - 2];

	while (hufPos->child1) {
		if (bitIn()) {
			hufPos = hufPos->child2;
		} else {
			hufPos = hufPos->child1;
		}
	}
	byte cpdIndex = byte(hufPos - hufTbl);
	++tblSizes[cpdIndex];

	int strPos;
	if (cpdBmask[cpdIndex] >= 256) {
		byte strPosLsb = charIn();
		byte strPosMsb = 0;
		for (byte nrBits = cpdExt[cpdIndex] - 8; nrBits--;
		     strPosMsb |= (bitIn() ? 1 : 0)) {
			strPosMsb <<= 1;
		}
		strPos = strPosLsb + 256 * strPosMsb;
	} else {
		strPos = 0;
		for (byte nrBits = cpdExt[cpdIndex]; nrBits--;
		     strPos |= (bitIn() ? 1 : 0)) {
			strPos <<= 1;
		}
	}
	if ((updHufCnt--) == 0) {
		mkHufTbl();	// make the huffman table
	}
	return strPos + cpDist[cpdIndex];
}

// read a bit from the input file
bool XSAExtractor::bitIn()
{
	if (bitCnt == 0) {
		bitFlg = charIn();	// read bitFlg
		bitCnt = 8;		// 8 bits left
	}
	bool temp = bitFlg & 1;
	--bitCnt;			// 1 bit less
	bitFlg >>= 1;

	return temp;
}

// initialize the huffman info tables
void XSAExtractor::initHufInfo()
{
	int offs = 1;
	for (int i = 0; i != TBLSIZE; ++i) {
		cpDist[i] = offs;
		cpdBmask[i] = 1 << cpdExt[i];
		offs += cpdBmask[i];
	}
	cpDist[TBLSIZE] = offs;

	for (int i = 0; i != TBLSIZE; ++i) {
		tblSizes[i] = 0;            // reset the table counters
		hufTbl[i].child1 = nullptr; // mark the leave nodes
	}
	mkHufTbl();	// make the huffman table
}

// Make huffman coding info
void XSAExtractor::mkHufTbl()
{
	// Initialize the huffman tree
	HufNode* hufPos = hufTbl;
	for (int i = 0; i != TBLSIZE; ++i) {
		(hufPos++)->weight = 1 + (tblSizes[i] >>= 1);
	}
	for (int i = TBLSIZE; i != 2 * TBLSIZE - 1; ++i) {
		(hufPos++)->weight = -1;
	}
	// Place the nodes in the correct manner in the tree
	while (hufTbl[2 * TBLSIZE - 2].weight == -1) {
		HufNode* l1Pos;
		HufNode* l2Pos;
		for (hufPos = hufTbl; !(hufPos->weight); ++hufPos) {
			// nothing
		}
		l1Pos = hufPos++;
		while (!(hufPos->weight)) {
			++hufPos;
		}
		if (hufPos->weight < l1Pos->weight) {
			l2Pos = l1Pos;
			l1Pos = hufPos++;
		} else {
			l2Pos = hufPos++;
		}
		int tempW;
		while ((tempW = (hufPos)->weight) != -1) {
			if (tempW) {
				if (tempW < l1Pos->weight) {
					l2Pos = l1Pos;
					l1Pos = hufPos;
				} else if (tempW < l2Pos->weight) {
					l2Pos = hufPos;
				}
			}
			++hufPos;
		}
		hufPos->weight = l1Pos->weight + l2Pos->weight;
		(hufPos->child1 = l1Pos)->weight = 0;
		(hufPos->child2 = l2Pos)->weight = 0;
	}
	updHufCnt = MAXHUFCNT;
}

} // namespace openmsx
