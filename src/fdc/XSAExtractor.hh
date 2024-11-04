#ifndef XSAEXTRACTOR_HH
#define XSAEXTRACTOR_HH

#include "DiskImageUtils.hh"
#include "MemBuffer.hh"

#include <array>
#include <cstdint>
#include <utility>

namespace openmsx {

class File;

class XSAExtractor
{
public:
	explicit XSAExtractor(File& file);
	std::pair<MemBuffer<SectorBuffer>, unsigned> extractData();

private:
	static constexpr int MAX_STR_LEN = 254;
	static constexpr int TBL_SIZE = 16;
	static constexpr int MAX_HUF_CNT = 127;

	[[nodiscard]] inline uint8_t charIn();
	void chkHeader();
	void unLz77();
	[[nodiscard]] unsigned rdStrLen();
	[[nodiscard]] int rdStrPos();
	[[nodiscard]] bool bitIn();
	void initHufInfo();
	void mkHufTbl();

	struct HufNode {
		HufNode* child1;
		HufNode* child2;
		int weight;
	};

private:
	MemBuffer<SectorBuffer> outBuf;	// the output buffer
	std::span<const uint8_t>::iterator inBufPos;	// pos in input buffer
	std::span<const uint8_t>::iterator inBufEnd;
	unsigned sectors;

	int updHufCnt;
	std::array<int, TBL_SIZE + 1> cpDist;
	std::array<int, TBL_SIZE> tblSizes;
	std::array<HufNode, 2 * TBL_SIZE - 1> hufTbl;

	uint8_t bitFlg;		// flag with the bits
	uint8_t bitCnt;		// nb bits left

	static constexpr std::array<uint8_t, TBL_SIZE> cpdExt = { // Extra bits for distance codes
		  0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12
	};
};

} // namespace openmsx

#endif
