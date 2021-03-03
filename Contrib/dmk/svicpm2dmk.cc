#include <string>
#include <stdexcept>
#include <vector>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <sys/stat.h>
#include <sys/types.h>


static const unsigned int TRACK_LENGTH = 6250;

struct DiskInfo
{
	int gap1;
	int gap2;
	int gap3;
	int gap4a;
	int sectorsPerTrack;
	int numberCylinders;
	int sectorSizeCode;
};

struct DmkHeader
{
	uint8_t writeProtected;
	uint8_t numTracks;
	uint8_t trackLen[2];
	uint8_t flags;
	uint8_t reserved[7];
	uint8_t format[4];
};


class File
{
public:
	File(const std::string& filename, const char* mode)
		: f(fopen(filename.c_str(), mode))
	{
		if (!f) {
			throw std::runtime_error("Couldn't open: " + filename);
		}
	}

	~File()
	{
		fclose(f);
	}

	void read(void* data, int size)
	{
		if (fread(data, size, 1, f) != 1) {
			throw std::runtime_error("Couldn't read file");
		}
	}
	void write(const void* data, int size)
	{
		if (fwrite(data, size, 1, f) != 1) {
			throw std::runtime_error("Couldn't write file");
		}
	}

private:
	FILE* f;
};


static void updateCrc(uint16_t& crc, uint8_t val)
{
	for (int i = 8; i < 16; ++i) {
		crc = (crc << 1) ^ ((((crc ^ (val << i)) & 0x8000) ? 0x1021 : 0));
	}
}

static void fill(uint8_t*& p, int len, uint8_t value)
{
	memset(p, value, len);
	p += len;
}

void convert(const DiskInfo& info, const std::string& input, const std::string& output)
{
	// Single or double sided input image?
	struct stat st;
	stat(input.c_str(), &st);
	int numSides;
	switch (st.st_size) {
		case 174080:
			numSides = 1;
			break;
		case 348160:
			numSides = 2;
			break;
		default:
			throw std::runtime_error(
				"input filesize should be exactly 174080 or 348160 bytes.\n");
	}

	int sectorSize = 128 << info.sectorSizeCode;
	int totalTracks = numSides * info.numberCylinders;
	int totalSectors = totalTracks * info.sectorsPerTrack;
	int totalSize = totalSectors * sectorSize;

	// sanity check
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
	header.flags = ((numSides == 2) ? (0 << 4) : (1 << 4)) |
	               (0 << 6); // double density (MFM)
	outf.write(&header, sizeof(header));

	std::vector<uint8_t*> addrPos(info.sectorsPerTrack);
	std::vector<uint8_t*> dataPos(info.sectorsPerTrack);
	std::vector<uint8_t> buf(dmkTrackLen); // zero-initialized
	uint8_t* ip = &buf[  0]; // pointer in IDAM table
	uint8_t* tp = &buf[128]; // pointer in actual track data

	fill(tp, info.gap4a, 0x4e); // gap4a
	fill(tp,         12, 0x00); // sync
	fill(tp,          3, 0xc2); // index mark
	fill(tp,          1, 0xfc); //
	fill(tp, info.gap1,  0x4e); // gap1
	for (int sec = 0; sec < info.sectorsPerTrack; ++sec) {
		fill(tp,         12, 0x00); // sync
		fill(tp,          3, 0xa1); // ID addr mark
		int pos = tp - &buf[0];
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
	assert((tp - &buf[0]) == dmkTrackLen);

	for (int cyl = 0; cyl < info.numberCylinders; ++cyl) {
		for (int head = 0; head < numSides; ++head) {
			for (int sec = 0; sec < info.sectorsPerTrack; ++sec) {
				uint8_t* ap = addrPos[sec];
				*ap++ = cyl;
				*ap++ = head;
				*ap++ = sec + 1;
				*ap++ = info.sectorSizeCode;

				uint16_t addrCrc = 0xffff;
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
			outf.write(&buf[0], dmkTrackLen);
		}
	}
}

int main(int argc, char** argv)
{
	if (argc != 3) {
		printf("svicpm2dmk\n"
			"---------\n"
			"\n"
			"Utility to convert a SVI CP/M disk image into a DMK disk image.\n"
			"\n"
			"The SVI CP/M image is expected to have to following format:\n"
			" - All tracks contain 17 sectors of 256 bytes.\n"
			" - There are 40 tracks on the disk.\n"
			" - The disk can be either single or double sided.\n"
			"This means that the size of the input image should be exactly\n"
			"174080 bytes for single sided disks or 348160 bytes for double\n"
			"sided disks.\n"
			"\n"
			"Usage: %s <input.dsk> <output.dmk>\n", argv[0]);
		exit(1);
	}

	// TODO add command line options to make these parameters configurable
	DiskInfo info;
	info.gap1  =  50;
	info.gap2  =  22;
	info.gap3  =  34;
	info.gap4a =  80;
	info.sectorsPerTrack = 17;
	info.numberCylinders = 40;
	info.sectorSizeCode = 1; // 256 = 128 << 1

	try {
		convert(info, argv[1], argv[2]);
	} catch (std::exception& e) {
		fprintf(stderr, "Error: %s\n", e.what());
	}
}
