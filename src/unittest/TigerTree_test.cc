#include "catch.hpp"
#include "TigerTree.hh"
#include <cstring>

using namespace openmsx;

struct TTTestData : public TTData
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
	uint8_t buffer_[8192 + 1];
	uint8_t* buffer = buffer_ + 1;
	TTTestData data;
	data.buffer = buffer;
	
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
		memset(buffer, 0, 100);
		CHECK(tt.calcHash(dummyCallback).toString() ==
		      "EOIEKIQO6BSNCNRX2UB2MB466INV6LICZ6MPUWQ");
		memset(buffer + 20, 1, 10);
		tt.notifyChange(20, 10, dummyTime);
		CHECK(tt.calcHash(dummyCallback).toString() ==
		      "GOTZVYW3WIE37XFCDOY66PLLXWGP6DPN3CQRHWA");
	}
	SECTION("3 full and 1 partial block") {
		TigerTree tt(data, 4000, dummyName);
		memset(buffer, 0, 4000);
		CHECK(tt.calcHash(dummyCallback).toString() ==
		       "YC44NFWFCN3QWFRSS6ICGUJDLH7F654RCKVT7VY");
		memset(buffer + 1500, 1, 10);
		tt.notifyChange(1500, 10, dummyTime); // change a single block
		CHECK(tt.calcHash(dummyCallback).toString() ==
		       "JU5RYR446PVZSPMOJML4IQ2FXLDDKE522CEYIBA");
		memset(buffer + 2000, 1, 100);
		tt.notifyChange(2000, 100, dummyTime); // change 2 blocks
		CHECK(tt.calcHash(dummyCallback).toString() ==
		       "IPV53CDVB2I63HXIXVK2OUPNS26YB7V7G2Y7XIA");
	}
	SECTION("7 full blocks (unbalanced internal binary tree)") {
		TigerTree tt(data, 7 * 1024, dummyName);
		memset(buffer, 0, 7 * 1024);
		CHECK(tt.calcHash(dummyCallback).toString() ==
		       "FPSZ35773WS4WGBVXM255KWNETQZXMTEJGFMLTA");
		memset(buffer + 512, 1, 512);
		tt.notifyChange(512, 512, dummyTime); // part of block-0
		CHECK(tt.calcHash(dummyCallback).toString() ==
		       "Z32BC2WSHPW5DYUSNSZGLDIFTEIP3DBFJ7MG2MQ");
		memset(buffer + 3 * 1024, 1, 4 * 1024);
		tt.notifyChange(3 * 1024, 4 * 1024, dummyTime); // blocks 3-6
		CHECK(tt.calcHash(dummyCallback).toString() ==
		       "SJUYB3QVIJXNKZMSQZGIMHA7GA2MYU2UECDA26A");
	}
}
