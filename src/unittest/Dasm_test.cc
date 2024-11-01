#include "catch.hpp"

#include "Dasm.hh"

#include <iostream>
#include <set>

using namespace openmsx;

TEST_CASE("instructionLength, dasm")
{
	std::set<std::string, std::less<>> allInstructions;
	std::array<uint8_t, 4> opcode = {0, 0, 0, 0};
	std::string dasmString;

	unsigned count = 0;
	bool end = false;
	while (!end) {
		++count;

		auto instrLen = instructionLength(opcode);
		REQUIRE(instrLen);
		REQUIRE(1 <= *instrLen);
		REQUIRE(*instrLen <= 4);

		dasmString.clear();
		auto dasmLen = dasm(std::span(opcode.data(), *instrLen), 0x1234, dasmString);
		CHECK(dasmLen == *instrLen);

		// check for duplicate instructions
		auto [it, inserted] = allInstructions.insert(dasmString);
		CHECK(inserted);

		if (dasmLen != *instrLen || !inserted) {
			std::string info;
			for (auto i : xrange(*instrLen)) {
				strAppend(info, ' ', hex_string<2>(opcode[i]));
			}
			std::cout << "opcode:" << info << " (" << dasmString << ")\n";
		}

		// goto next instruction
		unsigned idx = *instrLen - 1;
		while (true) {
			if (opcode[idx] == 255) {
				opcode[idx] = 0;
				if (idx == 0) {
					end = true;
					break;
				}
				--idx;
			} else {
				++opcode[idx];
				break;
			}
		}
	}
	REQUIRE(count == 2'904'196); // found experimentally
}
