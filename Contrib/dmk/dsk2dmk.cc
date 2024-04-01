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
#include <vector>

static constexpr unsigned TRACK_LENGTH = 6250;

struct DiskInfo
{
	int gap1;
	int gap2;
	int gap3;
	int gap4a;
	int sectorsPerTrack;
	int numberCylinders;
	int sectorSizeCode;
	bool doubleSided;
};

static void fill(uint8_t*& p, int len, uint8_t value)
{
	memset(p, value, len);
	p += len;
}

void convert(const DiskInfo& info, const std::string& input, const std::string& output)
{
	int numSides = info.doubleSided ? 2 : 1;
	int sectorSize = 128 << info.sectorSizeCode;
	int totalTracks = numSides * info.numberCylinders;
	int totalSectors = totalTracks * info.sectorsPerTrack;
	int totalSize = totalSectors * sectorSize;

	struct stat st;
	stat(input.c_str(), &st);
	if (st.st_size != totalSize) {
		throw std::runtime_error("Wrong input filesize");
	}

	File inf(input, "rb");
	File outf(output, "wb");

	int rawSectorLen =
		12 + 10 + info.gap2 + 12 + 4 +
		sectorSize + 2 + info.gap3;

	int gap4b = TRACK_LENGTH - (info.gap4a + 12 + 4 + info.gap1 +
		info.sectorsPerTrack * rawSectorLen);
	assert(gap4b > 0);
	int dmkTrackLen = TRACK_LENGTH + 128;

	DmkHeader header;
	memset(&header, 0, sizeof(header));
	header.numTracks = info.numberCylinders;
	header.trackLen[0] = dmkTrackLen & 0xff;
	header.trackLen[1] = dmkTrackLen >> 8;
	header.flags = (info.doubleSided ? (0 << 4) : (1 << 4)) |
	               (0 << 6); // double density (MFM)
	outf.write(&header, sizeof(header));

	std::vector<uint8_t*> addrPos(info.sectorsPerTrack);
	std::vector<uint8_t*> dataPos(info.sectorsPerTrack);
	std::vector<uint8_t> buf(dmkTrackLen); // zero-initialized
	uint8_t* ip = buf.data() +   0; // pointer in IDAM table
	uint8_t* tp = buf.data() + 128; // pointer in actual track data

	fill(tp, info.gap4a, 0x4e); // gap4a
	fill(tp,         12, 0x00); // sync
	fill(tp,          3, 0xc2); // index mark
	fill(tp,          1, 0xfc); //
	fill(tp, info.gap1,  0x4e); // gap1
	for (int sec = 0; sec < info.sectorsPerTrack; ++sec) {
		fill(tp,         12, 0x00); // sync
		fill(tp,          3, 0xa1); // ID addr mark
		int pos = tp - buf.data();
		assert(pos < 0x4000);
		*ip++ = pos & 0xff;
		*ip++ = (pos >> 8) | 0x80; // double density (MFM) sector
		fill(tp,          1, 0xfe); // ID addr mark (cont)
		addrPos[sec] = tp;
		fill(tp,          6, 0x00); // C H R N CRC (overwritten later)
		fill(tp, info.gap2,  0x4e); // gap2
		fill(tp,         12, 0x00); // sync
		fill(tp,          3, 0xa1); // data mark
		fill(tp,          1, 0xfb); //
		dataPos[sec] = tp;
		fill(tp, sectorSize, 0x00); // sector data (overwritten later)
		fill(tp,          2, 0x00); // CRC (overwritten later)
		fill(tp, info.gap3,  0x4e); // gap3
	}
	fill(tp, gap4b, 0x4e); // gap4b
	assert((tp - buf.data()) == dmkTrackLen);

	for (int cyl = 0; cyl < info.numberCylinders; ++cyl) {
		for (int head = 0; head < numSides; ++head) {
			for (int sec = 0; sec < info.sectorsPerTrack; ++sec) {
				uint8_t* ap = addrPos[sec];
				*ap++ = cyl;
				*ap++ = head;
				*ap++ = sec + 1;
				*ap++ = info.sectorSizeCode;

				uint16_t addrCrc = 0xffff;
				assert(ap >= &buf[8]);
				const uint8_t* t1 = ap - 8;
				for (int i = 0; i < 8; ++i) {
					updateCrc(addrCrc, t1[i]);
				}
				*ap++ = addrCrc >> 8;
				*ap++ = addrCrc & 0xff;

				uint8_t* dp = dataPos[sec];
				inf.read(dp, sectorSize);
				dp += sectorSize;

				uint16_t dataCrc = 0xffff;
				const uint8_t* t2 = dp - sectorSize - 4;
				for (int i = 0; i < sectorSize + 4; ++i) {
					updateCrc(dataCrc, t2[i]);
				}
				*dp++ = dataCrc >> 8;
				*dp++ = dataCrc & 0xff;
			}
			outf.write(buf.data(), dmkTrackLen);
		}
	}
}

int main(int argc, const char** argv)
{
	std::span arg(argv, argc);
	if (arg.size() != 3) {
		printf("dsk2dmk\n"
		       "\n"
		       "Utility to convert a dsk disk image into a dmk disk\n"
		       "image. At the moment this utility is limited to 720kB\n"
		       "double sided, double density dsk images.\n"
		       "\n"
		       "Usage: %s <input.dsk> <output.dmk>\n", arg[0]);
		exit(1);
	}

	// TODO add command line options to make these parameters configurable
	DiskInfo info;
	info.gap1  =  50;
	info.gap2  =  22;
	info.gap3  =  84;
	info.gap4a =  80;
	info.sectorsPerTrack = 9;
	info.numberCylinders = 80;
	info.sectorSizeCode = 2; // 512 = 128 << 2
	info.doubleSided = true;

	try {
		convert(info, arg[1], arg[2]);
	} catch (std::exception& e) {
		fprintf(stderr, "Error: %s\n", e.what());
	}
}
