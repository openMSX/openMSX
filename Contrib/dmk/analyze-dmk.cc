#include "dmk-common.hh"

#include <cassert>
#include <cstdlib>
#include <span>
#include <string>
#include <vector>


[[nodiscard]] static int modulo(int x, int y) {
	int z = x % y; // this calculates the remainder, not modulo!
	if (z < 0) z += y;
	return z;
}

struct DmkTrackData {
public:
	void read(File& file, int dmkTrackLen) {
		assert(dmkTrackLen > 128);
		buffer.resize(dmkTrackLen);
		file.read(buffer.data(), dmkTrackLen);
	}
	[[nodiscard]] int readDmkIndex(unsigned i) const {
		assert(i < 64);
		return buffer[2 * i + 0] + 256 * buffer[2 * i + 1];

	}
	[[nodiscard]] uint8_t readCircular(int idx) const
	{
		return buffer[128 + trackIdx(idx)];
	}
	[[nodiscard]] int trackIdx(int idx) const {
		return modulo(idx, trackLength());
	}
	[[nodiscard]] int trackLength() const {
		assert(buffer.size() > 128);
		return int(buffer.size() - 128);
	}

private:
	std::vector<uint8_t> buffer;
};



static bool isValidDmkHeader(const DmkHeader& header)
{
	if ((header.writeProtected != 0x00) &&
	    (header.writeProtected != 0xff)) {
		return false;
	}
	int trackLen = header.trackLen[0] + 256 * header.trackLen[1];
	if (trackLen >= 0x4000) return false; // too large track length
	if (trackLen <= 128)    return false; // too small
	if (header.flags & ~0xd0) return false; // unknown flag set
	for (int i = 0; i < 7 + 4; ++i) {
		if (header.reserved[i] != 0) return false;
	}
	return true;
}


static void analyzeTrack(const DmkTrackData& track)
{
	for (int i = 0; i < 64; ++i) {
		// Get (and check) pointer into track data
		int dmkIdx = track.readDmkIndex(i);
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
		if ((dmkIdx < 128) || ((dmkIdx + 128) >= track.trackLength())) {
			printf("... skipping invalid IDAM offset (wrong DMK file)\n");
			continue;
		}
		dmkIdx -= 128;

		// read (and check) address mark
		int addrIdx = track.trackIdx(dmkIdx - 3);
		uint8_t d0 = track.readCircular(addrIdx + 0);
		uint8_t d1 = track.readCircular(addrIdx + 1);
		uint8_t d2 = track.readCircular(addrIdx + 2);
		uint8_t d3 = track.readCircular(addrIdx + 3);
		uint8_t c  = track.readCircular(addrIdx + 4);
		uint8_t h  = track.readCircular(addrIdx + 5);
		uint8_t r  = track.readCircular(addrIdx + 6);
		uint8_t n  = track.readCircular(addrIdx + 7);
		uint8_t ch = track.readCircular(addrIdx + 8);
		uint8_t cl = track.readCircular(addrIdx + 9);

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
			uint8_t a0 = track.readCircular(dataIdx + 0);
			uint8_t a1 = track.readCircular(dataIdx + 1);
			uint8_t a2 = track.readCircular(dataIdx + 2);
			uint8_t t  = track.readCircular(dataIdx + 3);
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
			for (int k = 0; k < sectorSize; ++k) {
				uint8_t d = track.readCircular(dataIdx + 4 + k);
				updateCrc(dataCrc, d);
			}

			uint8_t crc1 = track.readCircular(dataIdx + 4 + sectorSize + 0);
			uint8_t crc2 = track.readCircular(dataIdx + 4 + sectorSize + 1);
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
	int dmkTrackLen = header.trackLen[0] + 256 * header.trackLen[1];

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

	DmkTrackData track;
	for (int t = 0; t < numCylinders; ++t) {
		for (int h = 0; h < numSides; ++h) {
			printf("-- physical track %d, head %d\n", t, h);
			track.read(inf, dmkTrackLen);
			analyzeTrack(track);
		}
	}
}

int main(int argc, const char** argv)
{
	std::span arg(argv, argc);
	if (arg.size() != 2) {
		printf("analyze-dmk\n"
		       "\n"
		       "Analyze the content of a DMK disk image.\n"
		       "\n"
		       "usage: %s <file.dmk>\n", arg[0]);
		exit(1);
	}
	try {
		analyzeDisk(arg[1]);
	} catch (std::exception& e) {
		fprintf(stderr, "Error: %s\n", e.what());
	}
}
