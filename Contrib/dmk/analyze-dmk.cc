#include <string>
#include <stdexcept>
#include <vector>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>


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

private:
	FILE* f;
};


// global variables for circular buffer
std::vector<uint8_t> buffer;
int dmkTrackLen;

static int modulo(int x, int y)
{
	int z = x % y; // this calculates the remainder, not modulo!
	if (z < 0) z += y;
	return z;
}

static int trackIdx(int idx)
{
	return modulo(idx, dmkTrackLen - 128);
}

static uint8_t readCircular(int idx)
{
	return buffer[128 + trackIdx(idx)];
}

static void updateCrc(uint16_t& crc, uint8_t val)
{
	for (int i = 8; i < 16; ++i) {
		crc = (crc << 1) ^ ((((crc ^ (val << i)) & 0x8000) ? 0x1021 : 0));
	}
}

static bool isValidDmkHeader(const DmkHeader& header)
{
	if (!((header.writeProtected == 0x00) ||
	      (header.writeProtected == 0xff))) {
		return false;
	}
	int trackLen = header.trackLen[0] + 256 * header.trackLen[1];
	if (trackLen >= 0x4000) return false; // too large track length
	if (trackLen <= 128)    return false; // too small
	if (header.flags & ~0xd0) return false; // unknown flag set
	const uint8_t* p = header.reserved;
	for (int i = 0; i < 7 + 4; ++i) {
		if (p[i] != 0) return false;
	}
	return true;
}


static void analyzeTrack()
{
	for (int i = 0; i < 64; ++i) {
		// Get (and check) pointer into track data
		int dmkIdx = buffer[2 * i + 0] + 256 * buffer[2 * i + 1];
		if (dmkIdx == 0) {
			// end of table reached
			break;
		}
		printf("%2d: ", i);
		if ((dmkIdx & 0xC000) != 0x8000) {
			printf("... skipping single-density sector\n");
			continue;
		}
		dmkIdx &= ~0xC000; // clear flags
		if ((dmkIdx < 128) || (dmkIdx >= dmkTrackLen)) {
			printf("... skipping invalid IDAM offset (wrong DMK file)\n");
			continue;
		}
		dmkIdx -= 128;

		// read (and check) address mark
		int addrIdx = trackIdx(dmkIdx - 3);
		uint8_t d0 = readCircular(addrIdx + 0);
		uint8_t d1 = readCircular(addrIdx + 1);
		uint8_t d2 = readCircular(addrIdx + 2);
		uint8_t d3 = readCircular(addrIdx + 3);
		uint8_t c  = readCircular(addrIdx + 4);
		uint8_t h  = readCircular(addrIdx + 5);
		uint8_t r  = readCircular(addrIdx + 6);
		uint8_t n  = readCircular(addrIdx + 7);
		uint8_t ch = readCircular(addrIdx + 8);
		uint8_t cl = readCircular(addrIdx + 9);

		if ((d0 != 0xA1) || (d1 != 0xA1) || (d2 != 0xA1) || (d3 != 0xFE)) {
			printf("... skipping wrong IDAM entry, does not point to an address mark\n");
			continue;
		}

		// address mark CRC
		uint16_t addrCrc = 0xFFFF;
		updateCrc(addrCrc, d0);
		updateCrc(addrCrc, d1);
		updateCrc(addrCrc, d2);
		updateCrc(addrCrc, d3);
		updateCrc(addrCrc, c);
		updateCrc(addrCrc, h);
		updateCrc(addrCrc, r);
		updateCrc(addrCrc, n);
		int onDiskAddrCrc = 256 * ch + cl;
		bool addrCrcErr = onDiskAddrCrc != addrCrc;

		// print address mark info
		printf("AOfst=%4d C=%3d H=%3d R=%3d N=%3d ACrc=%04x,%s",
		       addrIdx, c, h, r, n, onDiskAddrCrc,
		       (addrCrcErr ? "ERR\n" : "ok "));
		if (onDiskAddrCrc != addrCrc) {
			continue;
		}

		// locate data mark, should be within 43 bytes from end
		// of address mark (according to WD2793 datasheet)
		int j;
		for (j = 10; j < 53; ++j) {
			int dataIdx = addrIdx + j;
			uint8_t a0 = readCircular(dataIdx + 0);
			uint8_t a1 = readCircular(dataIdx + 1);
			uint8_t a2 = readCircular(dataIdx + 2);
			uint8_t t  = readCircular(dataIdx + 3);
			if ((a0 != 0xA1) || (a1 != 0xA1) || (a2 != 0xA1)) {
				continue;
			}

			// calculate data CRC, data mark part
			uint16_t dataCrc = 0xFFFF;
			updateCrc(dataCrc, a0);
			updateCrc(dataCrc, a1);
			updateCrc(dataCrc, a2);
			updateCrc(dataCrc, t);

			// actual sector data
			int sectorSize = 128 << (n & 7);
			for (int j = 0; j < sectorSize; ++j) {
				uint8_t d = readCircular(dataIdx + 4 + j);
				updateCrc(dataCrc, d);
			}

			uint8_t crc1 = readCircular(dataIdx + 4 + sectorSize + 0);
			uint8_t crc2 = readCircular(dataIdx + 4 + sectorSize + 1);
			int onDiskDataCrc = 256 * crc1 + crc2;
			bool dataCrcErr = onDiskDataCrc != dataCrc;

			char type = (t == 0xFB) ? 'n' :
			            (t == 0xF8) ? 'd' :
			                          '?';
			printf(" DOfst=%4d T=%c DCrc=%04x,%s\n",
			       dataIdx, type, onDiskDataCrc,
			       (dataCrcErr ? "ERR" : "ok "));
			break;
		}
		if (j == 53) {
			printf(" data mark not found within 43 bytes from address mark\n");
		}
	}
}

static void analyzeDisk(const std::string& input)
{
	File inf(input, "rb");
	DmkHeader header;
	inf.read(&header, sizeof(header));
	if (!isValidDmkHeader(header)) {
		throw std::runtime_error("Invalid DMK header");
	}

	int numCylinders = header.numTracks;
	int numSides = (header.flags & 0x10) ? 1 : 2;
	dmkTrackLen = header.trackLen[0] + 256 * header.trackLen[1];
	buffer.resize(dmkTrackLen);

	printf("Legend:\n"
	       "  AOfst:  address mark offset in track\n"
	       "  C:      cylinder\n"
	       "  H:      head\n"
	       "  R:      record (sector number)\n"
	       "  N:      sector size  (in bytes: 128 << N)\n"
	       "  ACrc:   CRC value of the address block\n"
	       "  DOfst:  data mark offset in track\n"
	       "  T:      data mark type (n = normal, d = deleted, ? = unknown)\n"
	       "  DCrc:   CRC value of data block\n"
	       "\n");

	printf("Raw track length = %d bytes\n\n", dmkTrackLen - 128);

	for (int t = 0; t < numCylinders; ++t) {
		for (int h = 0; h < numSides; ++h) {
			printf("-- physical track %d, head %d\n", t, h);
			inf.read(&buffer[0], dmkTrackLen);
			analyzeTrack();
		}
	}
}

int main(int argc, char** argv)
{
	if (argc != 2) {
		printf("analyze-dmk\n"
		       "\n"
		       "Analyze the content of a DMK disk image.\n"
		       "\n"
		       "usage: %s <file.dmk>\n", argv[0]);
		exit(1);
	}
	try {
		analyzeDisk(argv[1]);
	} catch (std::exception& e) {
		fprintf(stderr, "Error: %s\n", e.what());
	}
}
