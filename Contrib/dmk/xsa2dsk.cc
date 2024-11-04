// Compile with:
//   g++ -std=c++20 -O3 -Wall -Wextra xsa2dsk.cc ../../src/fdc/XSAExtractor.cc -I../../src -I../../src/fdc -I../../src/utils -o xsa2dsk

#include "MSXException.hh"
#include "XSAExtractor.hh"

#include <bit>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <span>
#include <vector>

int main(int argc, const char** argv)
{
	auto arg = std::span(argv, argc);
	if (arg.size() != 3) {
		std::cerr << "Usage: xsa2dsk <xsa-input-file> <dsk-output-file>\n";
		exit(1);
	}

	// Read xsa file
	std::ifstream in(arg[1], std::ios::binary);
	if (!in.is_open()) {
		std::cerr << "Failed to open: " << arg[1] << '\n';
		exit(1);
	}
	std::vector<uint8_t> inBuf(std::istreambuf_iterator<char>{in},
	                           std::istreambuf_iterator<char>{});

	// Parse xsa file
	auto sectors = [&] {
		try {
			openmsx::XSAExtractor extractor(inBuf);
			return std::move(extractor).extractData();
		} catch (openmsx::MSXException& e) {
			std::cerr << "Failed to parse: " << arg[1] << ": " << e.getMessage() << '\n';
			exit(1);
		}
	}();
	const uint8_t* dskData = sectors.data()->raw.data();
	auto dskSize = sectors.size() * sizeof(openmsx::SectorBuffer);

	// Write dsk file
	std::ofstream out(arg[2], std::ios::binary);
	if (!out.is_open()) {
		std::cerr << "Failed to open: " << arg[2] << '\n';
		exit(1);
	}
	out.write(std::bit_cast<const char*>(dskData), dskSize);
}
