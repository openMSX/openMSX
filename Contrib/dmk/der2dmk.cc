#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <vector>


struct DiskInfo
{
	int gap1;
	int gap2;
	int gap3;
	int gap4a;
	int gap4b;
	int sectorsPerTrack;
	int numberCylinders;
	int sectorSizeCode;
	bool doubleSided;
};

struct DmkHeader
{
	uint8_t writeProtected;
	uint8_t numTracks;
	std::array<uint8_t, 2> trackLen;
	uint8_t flags;
	std::array<uint8_t, 7> reserved;
	std::array<uint8_t, 4> format;
};


class File
{
public:
	File(const File&) = delete;
	File(File&&) = delete;
	File& operator=(const File&) = delete;
	File& operator=(File&&) = delete;

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

void convert(const DiskInfo& info, const std::string& dsk,
             const std::string& der, const std::string& dmk)
{
	int numSides = info.doubleSided ? 2 : 1;
	int sectorSize = 128 << info.sectorSizeCode;
	int totalTracks = numSides * info.numberCylinders;
	int totalSectors = totalTracks * info.sectorsPerTrack;
	int totalSize = totalSectors * sectorSize;

	struct stat st;
	stat(dsk.c_str(), &st);
	if (st.st_size != totalSize) {
		throw std::runtime_error("Wrong input filesize");
	}

	File inf (dsk, "rb");
	File derf(der, "rb");
	File outf(dmk, "wb");

	static const char* const DER_HEADER = "DiskImage errors\r\n\032";
	std::array<char, 20> derHeader;
	derf.read(derHeader.data(), 20);
	if (memcmp(derHeader.data(), DER_HEADER, 20) != 0) {
		throw std::runtime_error("Invalid .der file.");
	}
	int derSize = (totalSectors + 7)  / 8;
	std::vector<uint8_t> derBuf(derSize);
	derf.read(derBuf.data(), derSize);

	int rawSectorLen =
		12 + 10 + info.gap2 + 12 + 4 +
		sectorSize + 2 + info.gap3;
	int rawTrackLen =
		info.gap4a + 12 + 4 + info.gap1 +
		info.sectorsPerTrack * rawSectorLen + info.gap4b;
	assert(rawTrackLen == 6250);
	int dmkTrackLen = rawTrackLen + 128;

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
	fill(tp, info.gap4b, 0x4e); // gap4b
	assert((tp - buf.data()) == dmkTrackLen);

	int derCount = 0;
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
				if (derBuf[derCount / 8] & (0x80 >> (derCount % 8))) {
					// create CRC error for this sector
					dataCrc ^= 0xffff;
				}
				++derCount;
				*dp++ = dataCrc >> 8;
				*dp++ = dataCrc & 0xff;
			}
			outf.write(buf.data(), dmkTrackLen);
		}
	}
}

int main(int argc, char** argv)
{
	if (argc != 4) {
		printf("der2dmk\n"
		       "\n"
		       "Utility to convert a dsk disk image with associated disk\n"
		       "error file into a dmk disk image. At the moment this utility\n"
		       "is limited to 720kB double sided, double density dsk images.\n"
		       "\n"
		       "Usage: %s <input.dsk> <input.der> <output.dmk>\n", argv[0]);
		exit(1);
	}

	// TODO add command line options to make these parameters configurable
	DiskInfo info;
	info.gap1  =  50;
	info.gap2  =  22;
	info.gap3  =  84;
	info.gap4a =  80;
	info.gap4b = 182; // TODO calculate from other values
	info.sectorsPerTrack = 9;
	info.numberCylinders = 80;
	info.sectorSizeCode = 2; // 512 = 128 << 2
	info.doubleSided = true;

	try {
		convert(info, argv[1], argv[2], argv[3]);
	} catch (std::exception& e) {
		fprintf(stderr, "Error: %s\n", e.what());
	}
}
