#include "catch.hpp"

#include "FileOperations.hh"
#include "LocalFile.hh"
#include "MappedFile.hh"
#include "scope_exit.hh"

#include <algorithm>
#include <array>
#include <span>

using namespace openmsx;

TEST_CASE("MappedFile preserves the local file position")
{
	std::string filename;
	auto temporary = FileOperations::openUniqueFile(FileOperations::getTempDir(), filename);
	REQUIRE(temporary);
	temporary.reset();
	scope_exit cleanup([&] { FileOperations::unlink(filename); });

	LocalFile file(filename, File::OpenMode::TRUNCATE);
	const std::array<uint8_t, 5> content = {0x12, 0x34, 0x56, 0x78, 0x9a};
	file.write(content);
	file.flush();

	for (size_t position : {size_t(0), size_t(2), content.size()}) {
		for (size_t extra : {size_t(0), size_t(3)}) {
			CAPTURE(position, extra);
			file.seek(position);
			// A file-pool hash calculation followed by ROM loading maps
			// the same file twice. Neither mapping may consume the file.
			for (int repeat = 0; repeat < 2; ++repeat) {
				CAPTURE(repeat);
				MappedFile<const uint8_t> mapped = file.mmap(extra, true);
				CHECK(file.getPos() == position);
				REQUIRE(mapped.size() == content.size() + extra);
				CHECK(std::ranges::equal(content, std::span(mapped.data(), content.size())));
				CHECK(std::all_of(mapped.begin() + content.size(), mapped.end(),
				                 [](uint8_t value) { return value == 0; }));
			}
		}
	}
}
