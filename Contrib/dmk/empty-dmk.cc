#include "dmk-common.hh"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <span>

static const int RAW_TRACK_SIZE = 6250; // 250kbps, 300rpm

int main(int argc, const char** argv)
{
	std::span arg(argv, argc);
	if (arg.size() != 2) {
		printf("empty-dmk\n"
		       "\n"
		       "Utility to create an empty DMK disk image.\n"
		       "The disk image is double sided and contains\n"
		       "80 unformatted tracks.\n"
		       "\n"
		       "usage: %s <filename>\n", arg[0]);
		exit(1);
	}

	FILE_t f(fopen(arg[1], "wb"));
	if (!f) {
		printf("Error opening file '%s' for writing.\n", arg[1]);
		exit(1);
	}

	DmkHeader header;
	memset(&header, 0, sizeof(header));
	header.numTracks = 80;
	header.trackLen[0] = (128 + RAW_TRACK_SIZE) & 255;
	header.trackLen[1] = (128 + RAW_TRACK_SIZE) >> 8;
	fwrite(&header, sizeof(header), 1, f.get());

	std::array<uint8_t, 128 + RAW_TRACK_SIZE> buf;
	memset(&buf[  0],    0,  128);
	memset(&buf[128], 0x4e, RAW_TRACK_SIZE);
	for (int i = 0; i < 2 * 80; ++i) {
		fwrite(buf.data(), buf.size(), 1, f.get());
	}
}
