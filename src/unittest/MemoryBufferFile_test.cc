#include "catch.hpp"
#include "MemoryBufferFile.hh"
#include "File.hh"
#include "xrange.hh"

using namespace openmsx;

TEST_CASE("MemoryBufferFile")
{
	uint8_t buffer[] = {
		1, 2, 3, 4, 5, 6, 7, 8, 9, 10
	};
	uint8_t tmp[100];

	// create memory-backed-file
	File file = memory_buffer_file(buffer);
	CHECK(file.getPos() == 0);
	CHECK(file.isReadOnly());
	CHECK(file.getSize() == 10);

	// seek and read small part
	file.seek(2);
	CHECK(file.getPos() == 2);
	file.read(tmp, 3);
	CHECK(file.getPos() == 5);
	CHECK(tmp[0] == 3);
	CHECK(tmp[1] == 4);
	CHECK(tmp[2] == 5);

	// seek beyond end of file is ok, but reading there is not
	file.seek(100);
	CHECK(file.getPos() == 100);
	CHECK_THROWS(file.read(tmp, 2));

	// try to read more than file size
	file.seek(0);
	CHECK(file.getPos() == 0);
	CHECK_THROWS(file.read(tmp, 100));
	CHECK(file.getPos() == 0);

	// read full file
	file.seek(0);
	file.read(tmp, file.getSize());
	for (auto i : xrange(10)) {
		CHECK(tmp[i] == (i + 1));
	}
	CHECK(file.getPos() == file.getSize());

	// writing is not supported
	CHECK_THROWS(file.write(tmp, file.getSize()));
}
