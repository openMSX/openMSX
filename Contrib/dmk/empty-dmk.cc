#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>

struct DmkHeader {
	uint8_t writeProtected;
	uint8_t numTracks;
	uint8_t trackLen[2];
	uint8_t flags;
	uint8_t reserved[7];
	uint8_t format[4];
};

static const int RAW_TRACK_SIZE = 6250; // 250kbps, 300rpm

int main(int argc, char** argv)
{
	if (argc != 2) {
		printf("empty-dmk\n"
		       "\n"
		       "Utility to create an empty DMK disk image.\n"
		       "The disk image is double sided and contains\n"
		       "80 unformatted tracks.\n"
		       "\n"
		       "usage: %s <filename>\n", argv[0]);
		exit(1);
	}

	FILE* f = fopen(argv[1], "wb");

	DmkHeader header;
	memset(&header, 0, sizeof(header));
	header.numTracks = 80;
	header.trackLen[0] = (128 + RAW_TRACK_SIZE) & 255;
	header.trackLen[1] = (128 + RAW_TRACK_SIZE) >> 8;
	fwrite(&header, sizeof(header), 1, f);

	uint8_t buf[128 + RAW_TRACK_SIZE];
	memset(&buf[  0],    0,  128);
	memset(&buf[128], 0x4e, RAW_TRACK_SIZE);
	for (int i = 0; i < 2 * 80; ++i) {
		fwrite(buf, sizeof(buf), 1, f);
	}

	fclose(f);
}
