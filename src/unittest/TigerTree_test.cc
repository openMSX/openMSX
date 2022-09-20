#include "catch.hpp"
#include "TigerTree.hh"
#include "tiger.hh"
#include "ranges.hh"
#include <span>

using namespace openmsx;

struct TTTestData final : public TTData
{
	uint8_t* getData(size_t offset, size_t /*size*/) override
	{
		return buffer + offset;
	}

	bool isCacheStillValid(time_t&) override
	{
		return false;
	}

	uint8_t* buffer;
};


// TODO check that hash (re)calculation is indeed incremental

TEST_CASE("TigerTree")
{
	static constexpr auto BLOCK_SIZE = TigerTree::BLOCK_SIZE;
	std::array<uint8_t, 8 * BLOCK_SIZE + 1> buffer_;
	auto buffer = subspan(buffer_, 1);
	TTTestData data;
	data.buffer = buffer.data();

	std::string dummyName;
	time_t dummyTime = 0;
	auto dummyCallback = [](size_t, size_t) {};

	SECTION("zero sized buffer") {
		TigerTree tt(data, 0, dummyName);

		CHECK(tt.calcHash(dummyCallback).toString() ==
		      "LWPNACQDBZRYXW3VHJVCJ64QBZNGHOHHHZWCLNQ");
	}
	SECTION("size less than 1 block") {
		TigerTree tt(data, 100, dummyName);
		ranges::fill(subspan<100>(buffer), 0);
		CHECK(tt.calcHash(dummyCallback).toString() ==
		      "EOIEKIQO6BSNCNRX2UB2MB466INV6LICZ6MPUWQ");
		ranges::fill(subspan<10>(buffer, 20), 1);
		tt.notifyChange(20, 10, dummyTime);
		CHECK(tt.calcHash(dummyCallback).toString() ==
		      "GOTZVYW3WIE37XFCDOY66PLLXWGP6DPN3CQRHWA");
	}
	SECTION("3 full and 1 partial block") {
		TigerTree tt(data, 3 * BLOCK_SIZE + 500, dummyName);
		ranges::fill(subspan<3 * BLOCK_SIZE + 500>(buffer), 0);
		CHECK(tt.calcHash(dummyCallback).toString() ==
		      "K6NHCUINLFZ7OUMUZ44JSRABL5C62WTCY2BONUI");

		ranges::fill(subspan<10>(buffer, BLOCK_SIZE + 500), 1);
		tt.notifyChange(BLOCK_SIZE + 500, 10, dummyTime); // change a single block
		CHECK(tt.calcHash(dummyCallback).toString() ==
		      "WGRG4PY3CZDLYLTC6BZ2X3G22H6DEB77JH4XPBA");

		ranges::fill(subspan<100>(buffer, 2 * BLOCK_SIZE - 50), 1);
		tt.notifyChange(2 * BLOCK_SIZE - 50, 100, dummyTime); // change 2 blocks
		CHECK(tt.calcHash(dummyCallback).toString() ==
		      "Y55VG6WSVEPWGPRZGCB4L2OYLI7CLVGQ6X6J4MY");

	}
	SECTION("7 full blocks (unbalanced internal binary tree)") {
		TigerTree tt(data, 7 * BLOCK_SIZE, dummyName);
		ranges::fill(subspan<7 * BLOCK_SIZE>(buffer), 0);
		CHECK(tt.calcHash(dummyCallback).toString() ==
		      "FPSZ35773WS4WGBVXM255KWNETQZXMTEJGFMLTA");

		ranges::fill(subspan<500>(buffer, 500), 1);
		tt.notifyChange(500, 500, dummyTime); // part of block-0
		CHECK(tt.calcHash(dummyCallback).toString() ==
		      "NBCRBTHDNUDTAKZRMYO6TIQGQJIWX74BYNTYXBA");

		ranges::fill(subspan<4 * BLOCK_SIZE>(buffer, 3 * BLOCK_SIZE), 1);
		tt.notifyChange(3 * BLOCK_SIZE, 4 * BLOCK_SIZE, dummyTime); // blocks 3-6
		CHECK(tt.calcHash(dummyCallback).toString() ==
		      "PLHCYOTPV4TTXTUPHYGGVPMARGMFE4U5JYRV4VA");
	}
}
