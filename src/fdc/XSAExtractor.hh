#ifndef XSAEXTRACTOR_HH
#define XSAEXTRACTOR_HH

#include "DiskImageUtils.hh"

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace openmsx {

class XSAExtractor
{
public:
	explicit XSAExtractor(std::span<const uint8_t> file);
	std::vector<SectorBuffer> extractData() &&;

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
	std::span<const uint8_t> file; // the not-yet-consumed part of the file
	std::vector<SectorBuffer> output;

	int updHufCnt;
	std::array<int, TBL_SIZE + 1> cpDist;
	std::array<int, TBL_SIZE> tblSizes;
	std::array<HufNode, 2 * TBL_SIZE - 1> hufTbl;

	uint8_t bitFlg; // flag with the bits
	uint8_t bitCnt; // nb bits left

	static constexpr std::array<uint8_t, TBL_SIZE> cpdExt = { // Extra bits for distance codes
		  0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12
	};
};

} // namespace openmsx

#endif
