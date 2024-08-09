#include "Dasm.hh"

#include "catch.hpp"

#include <algorithm>
#include <iostream>
#include <set>

using namespace openmsx;

TEST_CASE("instructionLength, dasm")
{
	std::set<std::string> allInstructions;
	std::array<uint8_t, 4> opcode = {0, 0, 0, 0};
	std::string dasmString;

	unsigned count = 0;
	bool end = false;
	while (!end) {
		count++;

		auto instructionLength_ = instructionLength(opcode).value_or(0);
		REQUIRE(instructionLength_ <= 4);

		dasmString.clear();
		auto dasmLength = dasm(std::span(opcode.data(), instructionLength_), 0x1234, dasmString);

		CHECKED_ELSE(instructionLength_ == dasmLength) {
			std::stringstream ss;
			ss << std::hex;

			for (unsigned j = 0; j < opcode.size(); ++j) {
				ss << std::setw(2) << std::setfill('0') << int(opcode[j]) << ' ';
			}
			ss << '(' << dasmString << ")\n";
			INFO( "opcode: " << ss.str());

			CHECK(dasmLength == instructionLength_);
		}

		// check for duplicate instructions
		auto [it, inserted] = allInstructions.insert(dasmString);
		CHECK(inserted == true);

		// goto next instruction
		unsigned idx = std::min(instructionLength_ - 1, unsigned(0));
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
