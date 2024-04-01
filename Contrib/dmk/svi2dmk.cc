#include "dmk-common.hh"

#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <span>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>

static const int TRACK_LENGTH = 6250;
static const int DMK_TRACK_LEN = TRACK_LENGTH + 128;
static const int NUM_CYLINDERS = 40;
static const int GAP1  = 50;
static const int GAP2  = 22;
static const int GAP3  = 34;
static const int GAP4a = 80;


static uint16_t calculateCrc(const uint8_t* p, int n)
{
	uint16_t crc = 0xffff;
	for (int i = 0; i < n; ++i) {
		updateCrc(crc, p[i]);
	}
	return crc;
}

static void fill(uint8_t*& p, int len, uint8_t value)
{
	memset(p, value, len);
	p += len;
}

static void convertTrack(int cyl, int head,
                         int sectorsPerTrack, int sectorSizeCode, int gap3,
                         File& inf, File& outf)
{
	int sectorSize = 128 << sectorSizeCode;
	int rawSectorLen = 12 + 10 + GAP2 + 12 + 4 + sectorSize + 2 + gap3;
	int gap4b = TRACK_LENGTH - (GAP4a + 12 + 4 + GAP1 + sectorsPerTrack * rawSectorLen);
	assert(gap4b > 0);

	std::array<uint8_t, DMK_TRACK_LEN> buf = {};
	uint8_t* ip = &buf[  0]; // pointer in IDAM table
	uint8_t* tp = &buf[128]; // pointer in actual track data

	fill(tp, GAP4a, 0x4e); // gap4a
	fill(tp,    12, 0x00); // sync
	fill(tp,     3, 0xc2); // index mark
	fill(tp,     1, 0xfc); //
	fill(tp,  GAP1, 0x4e); // gap1
	for (int sec = 0; sec < sectorsPerTrack; ++sec) {
		fill(tp,         12, 0x00); // sync
		fill(tp,          3, 0xa1); // ID addr mark
		int pos = tp - &buf[0];
		assert(pos < 0x4000);
		*ip++ = pos & 0xff;
		*ip++ = (pos >> 8) | 0x80; // double density (MFM) sector
		fill(tp,          1, 0xfe); // ID addr mark (cont)
		*tp++ = cyl;                // C
		*tp++ = head;               // H
		*tp++ = sec + 1;            // R
		*tp++ = sectorSizeCode;     // N
		auto addrCrc = calculateCrc(tp - 8, 8);
		*tp++ = addrCrc >> 8;       // CRC-1
		*tp++ = addrCrc & 0xff;     // CRC-2
		fill(tp, GAP2, 0x4e);       // gap2
		fill(tp,   12, 0x00);       // sync
		fill(tp,    3, 0xa1);       // data mark
		fill(tp,    1, 0xfb);       //
		inf.read(tp, sectorSize);
		auto dataCrc = calculateCrc(tp - 4, sectorSize + 4);
		tp += sectorSize;
		*tp++ = dataCrc >> 8;
		*tp++ = dataCrc & 0xff;
		fill(tp, gap3,  0x4e);      // gap3
	}
	fill(tp, gap4b, 0x4e); // gap4b
	assert((tp - &buf[0]) == DMK_TRACK_LEN);

	outf.write(&buf[0], DMK_TRACK_LEN);
}

static void convert(const char* input, const char* output)
{
	// Single or double sided input image?
	struct stat st;
	stat(input, &st);
	bool doubleSided;
	switch (st.st_size) {
	case 172032:
		doubleSided = false;
		break;
	case 346112:
		doubleSided = true;
		break;
	default:
		throw std::runtime_error(
			"input filesize should be exactly 172032 or 346112 bytes.\n");
	}

	// Write DMK header
	DmkHeader header;
	memset(&header, 0, sizeof(header));
	header.numTracks = NUM_CYLINDERS;
	header.trackLen[0] = DMK_TRACK_LEN & 0xff;
	header.trackLen[1] = DMK_TRACK_LEN >> 8;
	header.flags = (doubleSided ? (0 << 4) : (1 << 4)) |
	               (0 << 6); // double density (MFM)
	File outf(output, "wb");
	outf.write(&header, sizeof(header));

	// Read input image and convert to DMK, track per track
	File inf(input, "rb");
	int numSides = doubleSided ? 2 : 1;
	for (int cyl = 0; cyl < NUM_CYLINDERS; ++cyl) {
		for (int head = 0; head < numSides; ++head) {
			// first track has different layout
			int sectorsPerTrack;
			int sectorSizeCode;
			int gap3;
			if ((head == 0) && (cyl == 0)) {
				sectorsPerTrack = 18;
				sectorSizeCode = 0; // 128 bytes
				gap3 = 84;
			} else {
				sectorsPerTrack = 17;
				sectorSizeCode = 1; // 256 bytes
				gap3 = 34;
			}
			convertTrack(cyl, head,
			             sectorsPerTrack, sectorSizeCode, gap3,
			             inf, outf);
		}
	}
}

int main(int argc, const char** argv)
{
	std::span arg(argv, argc);
	if (arg.size() != 3) {
		printf("svi2dmk\n"
		       "-------\n"
		       "\n"
		       "Utility to convert a SVI disk image into a DMK disk image.\n"
		       "\n"
		       "The SVI image is expected to have to following format:\n"
		       " - 1st track (track 0, side 0) contains 18 sectors of 128 bytes.\n"
		       " - All other tracks contain 17 sectors of 256 bytes.\n"
		       " - There are 40 tracks on the disk.\n"
		       " - The disk can be either single or double sided.\n"
		       "This means that the size of the input image should be exactly\n"
		       "172032 bytes for single sided disks or 346112 bytes for double\n"
		       "sided disks.\n"
		       "\n"
		       "Usage: %s <input.dsk> <output.dmk>\n", arg[0]);
		exit(1);
	}

	try {
		convert(arg[1], arg[2]);
	} catch (std::exception& e) {
		fprintf(stderr, "Error: %s\n", e.what());
		exit(2);
	}
}
