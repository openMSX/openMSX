#include "Dasm.hh"
#include "EmuTime.hh"
#include "MemInterface.hh"

#include "catch.hpp"

#include <iostream>

using namespace openmsx;

class DummyInterface: public MemInterface {
public:
	DummyInterface(std::array<uint8_t, 4>& buf) : buffer(buf) {}

	byte peekMem(word address, [[maybe_unused]] EmuTime::param time) const override
	{
		return buffer[address];
	}

	std::array<uint8_t, 4>& buffer;
};


TEST_CASE("instructionLength x fetchInstruction x dasm")
{
	std::array<uint8_t, 4> buf = {0, 0, 0, 0};
	std::span<uint8_t, 4> span{buf};

	DummyInterface interface{buf};
	std::string dest;

	for (uint32_t i = 0; i < UINT32_MAX; ++i) {
		buf[3] = narrow_cast<uint8_t>(i & 0xff);
		buf[2] = narrow_cast<uint8_t>((i >> 8) & 0xff);
		buf[1] = narrow_cast<uint8_t>((i >> 16) & 0xff);
		buf[0] = narrow_cast<uint8_t>((i >> 24) & 0xff);
		auto tmp = fetchInstruction(interface, 0, span, EmuTime::zero());
		auto len = instructionLength(span);
		CHECK(len == tmp.size());
		auto res = dasm(tmp, 0, dest);
		CHECK(len == res);
	}

	// last case: buf = 0xffffffff
	buf[0] = 0xff;
	auto len = instructionLength(span);
	auto tmp = fetchInstruction(interface, 0, span, EmuTime::zero());
	CHECK(len == tmp.size());
	auto res = dasm(tmp, 0, dest);
	CHECK(res == len);

}
