#include "XSAExtractor.hh"

#include "MSXException.hh"

#include "narrow.hh"
#include "xrange.hh"

namespace openmsx {

XSAExtractor::XSAExtractor(std::span<const uint8_t> file_)
	: file(file_)
{
	if ((charIn() != 'P') || (charIn() != 'C') ||
	    (charIn() != 'K') || (charIn() != '\010')) {
		throw MSXException("Not an XSA image");
	}

	chkHeader();
	initHufInfo();	// initialize the cpDist tables
	unLz77();
}

std::vector<SectorBuffer> XSAExtractor::extractData() &&
{
	// destroys internal outBuf, but that's ok
	return std::move(output);
}

// Get the next character from the input buffer
uint8_t XSAExtractor::charIn()
{
	if (file.empty()) {
		throw MSXException("Corrupt XSA image: unexpected end of file");
	}
	auto result = file.front();
	file = file.subspan(1);
	return result;
}

// check fileheader
void XSAExtractor::chkHeader()
{
	// read original length (little endian)
	unsigned outBufLen = 0;
	for (auto i : xrange(4)) {
		outBufLen |= charIn() << (8 * i);
	}
	auto sectors = (outBufLen + 511) / 512;
	output.resize(sectors);

	// skip compressed length
	repeat(4, [&]{ (void)charIn(); });

	// skip original filename
	while (charIn()) /*empty*/;
}

// the actual decompression algorithm itself
void XSAExtractor::unLz77()
{
	bitCnt = 0; // no bits read yet

	size_t remaining = output.size() * sizeof(SectorBuffer);
	std::span out{output.data()->raw.data(), remaining};
	size_t outIdx = 0;
	while (true) {
		if (bitIn()) {
			// 1-bit
			unsigned strLen = rdStrLen();
			if (strLen == (MAX_STR_LEN + 1)) {
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

	uint8_t nrBits = 2;
	while ((nrBits != 7) && bitIn()) {
		++nrBits;
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
	HufNode* hufPos = &hufTbl[2 * TBL_SIZE - 2];

	while (hufPos->child1) {
		if (bitIn()) {
			hufPos = hufPos->child2;
		} else {
			hufPos = hufPos->child1;
		}
	}
	auto cpdIndex = narrow<uint8_t>(hufPos - &hufTbl[0]);
	++tblSizes[cpdIndex];

	auto getNBits = [&](unsigned n) {
		assert(n <= 8);
		uint8_t result = 0;
		repeat(n, [&]{
			result = uint8_t((result << 1) | (bitIn() ? 1 : 0));
		});
		return result;
	};
	int strPos = [&] {
		if (cpdExt[cpdIndex] >= 8) {
			uint8_t strPosLsb = charIn();
			uint8_t strPosMsb = getNBits(narrow_cast<uint8_t>(cpdExt[cpdIndex] - 8));
			return strPosLsb + 256 * strPosMsb;
		} else {
			return int(getNBits(cpdExt[cpdIndex]));
		}
	}();
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
	for (auto i : xrange(TBL_SIZE)) {
		cpDist[i] = offs;
		offs += 1 << cpdExt[i];
	}
	cpDist[TBL_SIZE] = offs;

	for (auto i : xrange(TBL_SIZE)) {
		tblSizes[i] = 0;            // reset the table counters
		hufTbl[i].child1 = nullptr; // mark the leave nodes
	}
	mkHufTbl();	// make the huffman table
}

// Make huffman coding info
void XSAExtractor::mkHufTbl()
{
	// Initialize the huffman tree
	HufNode* hufPos = &hufTbl[0];
	for (auto i : xrange(TBL_SIZE)) {
		(hufPos++)->weight = 1 + (tblSizes[i] >>= 1);
	}
	for (int i = TBL_SIZE; i != 2 * TBL_SIZE - 1; ++i) {
		(hufPos++)->weight = -1;
	}
	// Place the nodes in the correct manner in the tree
	while (hufTbl[2 * TBL_SIZE - 2].weight == -1) {
		for (hufPos = &hufTbl[0]; !(hufPos->weight); ++hufPos) {
			// nothing
		}
		HufNode* l1Pos = hufPos++;
		while (!(hufPos->weight)) {
			++hufPos;
		}
		HufNode* l2Pos = [&] {
			if (hufPos->weight < l1Pos->weight) {
				auto* tmp = l1Pos;
				l1Pos = hufPos++;
				return tmp;
			} else {
				return hufPos++;
			}
		}();
		int tempW;
		while ((tempW = hufPos->weight) != -1) {
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
	updHufCnt = MAX_HUF_CNT;
}

} // namespace openmsx
