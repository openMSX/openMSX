#include <vector>
#include <map>
#include <algorithm>
#include <string>
#include <cstdint>
#include <cstdio>
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <sys/stat.h>

struct DmkHeader
{
	uint8_t writeProtected;
	uint8_t numTracks;
	uint8_t trackLen[2];
	uint8_t flags;
	uint8_t reserved[7];
	uint8_t format[4];
};

///////

class Gaps
{
public:
	Gaps(int totalSize);
	void addInterval(int start, int stop);
	int getLargestGap();
private:
	void addUse2(int start, int stop);

	const int totalSize;
	std::vector<int> v;
};

Gaps::Gaps(int totalSize_)
	: totalSize(totalSize_)
{
}

// [start, stop)
void Gaps::addInterval(int start, int stop)
{
	start %= totalSize;
	stop  %= totalSize;

	if (start < stop) {
		addUse2(start, stop);
	} else if (start > stop) {
		addUse2(start, totalSize);
		if (stop != 0) addUse2(0, stop);
	}
}
void Gaps::addUse2(int start, int stop)
{
	assert(start < stop);
	assert(stop <= totalSize);
	v.push_back(2 * start + 0);
	v.push_back(2 * stop  + 1);
}

int Gaps::getLargestGap()
{
	if (v.empty()) return totalSize / 2;

	// sort begin and end point
	std::sort(v.begin(), v.end());

	// largest gap found so far (and its start and stop position)
	int maxLen = 0;
	int maxStart = 0;
	int maxStop = 0;

	// last end point is start of first gap
	int start = v.back() / 2;
	if (start == totalSize) start = 0;

	int count = 0;
	for (int i : v) {
		if (i & 1) {
			// interval end point
			--count;
			// no more overlapping intervals -> start of new gap
			if (count == 0) start = i / 2;
		} else {
			// interval begin point
			int stop = i / 2;
			if (count == 0) {
				// found gap: [start, stop)
				int len = stop - start;
				if (len < 0) len += totalSize;
				if (len > maxLen) {
					maxLen   = len;
					maxStart = start;
					maxStop  = stop;
				}
			}
			++count;
		}
	}
	assert(count == 0);

	if (maxLen == 0) return -1;
	if (maxStop < maxStart) maxStop += totalSize;
	int mid = (maxStart + maxStop) / 2;
	return mid % totalSize;
}

///////


static uint8_t readCircular(const std::vector<uint8_t>& buffer, int idx)
{
	int dmkTrackLen = buffer.size();
	return buffer[128 + idx % (dmkTrackLen - 128)];
}

static void updateCrc(uint16_t& crc, uint8_t val)
{
	for (int i = 8; i < 16; ++i) {
		crc = (crc << 1) ^ ((((crc ^ (val << i)) & 0x8000) ? 0x1021 : 0));
	}
}

static void verifyDMK(bool b, const char* message)
{
	if (!b) {
		fprintf(stderr, "Invalid input: %s\n", message);
		exit(1);
	}
}

static int analyzeTrack(std::vector<uint8_t>& buffer)
{
	int dmkTrackLen = buffer.size();
	int trackLen = dmkTrackLen - 128;

	Gaps gaps(trackLen);

	for (int i = 0; i < 64; ++i) {
		// Get (and check) pointer into track data
		int dmkIdx = buffer[2 * i + 0] + 256 * buffer[2 * i + 1];
		if (dmkIdx == 0) {
			// end of table reached
			break;
		}
		verifyDMK((dmkIdx & 0xC000) == 0x8000, "double density flag");
		dmkIdx &= ~0xC000; // clear flags
		verifyDMK(dmkIdx >= 128, "IDAM offset too small");
		verifyDMK(dmkIdx < dmkTrackLen, "IDAM offset too large");
		dmkIdx -= 128;

		// read address mark
		int addrIdx = dmkIdx - 3; // might be negative
		uint8_t d0 = readCircular(buffer, addrIdx + 0);
		uint8_t d1 = readCircular(buffer, addrIdx + 1);
		uint8_t d2 = readCircular(buffer, addrIdx + 2);
		uint8_t d3 = readCircular(buffer, addrIdx + 3);
		uint8_t c  = readCircular(buffer, addrIdx + 4);
		uint8_t h  = readCircular(buffer, addrIdx + 5);
		uint8_t r  = readCircular(buffer, addrIdx + 6);
		uint8_t n  = readCircular(buffer, addrIdx + 7);
		uint8_t ch = readCircular(buffer, addrIdx + 8);
		uint8_t cl = readCircular(buffer, addrIdx + 9);

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
		uint16_t onDiskAddrCrc = 256 * ch + cl;

		if (onDiskAddrCrc != addrCrc) {
			// only mark address mark as in-use
			gaps.addInterval(addrIdx, addrIdx + 10);
			continue;
		}

		// locate data mark, should be within 43 bytes from end
		// of address mark (according to WD2793 datasheet)
		for (int i = 10; i < 53; ++i) {
			int dataIdx = addrIdx + i;
			uint8_t a0 = readCircular(buffer, dataIdx + 0);
			uint8_t a1 = readCircular(buffer, dataIdx + 1);
			uint8_t a2 = readCircular(buffer, dataIdx + 2);
			uint8_t t  = readCircular(buffer, dataIdx + 3);
			if ((a0 != 0xA1) || (a1 != 0xA1) || (a2 != 0xA1) ||
			    ((t != 0xFB) && (t != 0xF8))) {
				continue;
			}

			// Mark whole region from begin of address mark to end
			// of data as in-use (so including the gap between
			// address and data section).
			int sectorSize = 128 << (n & 3);
			int end = dataIdx + 4 + sectorSize + 2; // header + data + crc
			gaps.addInterval(addrIdx, end);
			break;
		}
		// if (i == 53) --> data mark not found
	}
	return gaps.getLargestGap();
}

int main()
{
	std::vector<std::vector<uint8_t>> data; // buffer all .DAT files

	std::string name = "DMK-tt-s.DAT";
	for (int t = 0; t <= 99; ++t) {
		for (int h = 0; h < 2; ++h) {
			name[4] = (t / 10) + '0';
			name[5] = (t % 10) + '0';
			name[7] = h + '0';

			FILE* file = fopen(name.c_str(), "rb");
			if (!file) {
				if (h == 0) goto done_read;
				fprintf(stderr,
					"Couldn't open file %s, but the "
					"corresponding file for side 0 was "
					"found.\n",
					name.c_str());
				exit(1);
			}

			struct stat st;
			fstat(fileno(file), &st);
			size_t size = st.st_size;
			if (size < 128) {
				fprintf(stderr, "File %s is too small.\n",
				        name.c_str());
				exit(1);
			}

			std::vector<uint8_t> dat(size);
			if (fread(dat.data(), size, 1, file) != 1) {
				fprintf(stderr, "Error reading file %s.\n",
				        name.c_str());
				exit(1);
			}
			data.push_back(dat);
		}
	}
done_read:
	assert((data.size() & 1) == 0);
	int numTracks = data.size() / 2;

	// Check that no .dat files with higher track number are found.
	for (int t = numTracks; t <= 99; ++t) {
		for (int h = 0; h < 2; ++h) {
			name[4] = (t / 10) + '0';
			name[5] = (t % 10) + '0';
			name[7] = h + '0';

			FILE* file = fopen(name.c_str(), "rb");
			if (!file) continue; // ok, we should have this file

			std::string name2 = "DMK-tt-0.DAT";
			name2[4] = (numTracks / 10) + '0';
			name2[5] = (numTracks % 10) + '0';
			fprintf(stderr,
				"Found file %s, but file %s is missing.\n",
				name.c_str(), name2.c_str());
			exit(1);
		}
	}

	printf("Found .dat files for %d tracks (double sided).\n", numTracks);

	// Create histogram of tracklengths.
	std::map<unsigned, unsigned> sizes; // length, count
	for (const auto& d : data) {
		++sizes[d.size()];
	}

	// Search the peak in this histogram (= the tracklength that occurs
	// most often).
	unsigned maxCount = 0;
	unsigned trackSize = 0;
	for (const auto& p : sizes) {
		if (p.second >= maxCount) {
			maxCount = p.second;
			trackSize = p.first;
		}
	}

	// Open output file.
	FILE* file = fopen("out.dmk", "wb+");
	if (!file) {
		fprintf(stderr, "Couldn't open output file out.dmk.\n");
		exit(1);
	}

	// Write DMK header.
	DmkHeader header;
	memset(&header, 0, sizeof(header));
	header.numTracks = numTracks;
	header.trackLen[0] = trackSize % 256;
	header.trackLen[1] = trackSize / 256;
	if (fwrite(&header, sizeof(header), 1, file) != 1) {
		fprintf(stderr, "Error writing out.dmk.\n");
		exit(1);
	}

	// Process each track.
	for (auto& v : data) {
		// Adjust track size
		while (v.size() != trackSize) {
			// Locate (middle of) largest gap in this track.
			int mid = analyzeTrack(v) + 128;
			if (mid == -1) {
				fprintf(stderr, "Couldn't adjust track length.\n");
				exit(1);
			}
			assert(mid < int(v.size()));

			// We insert or delete one byte at a time. This may not
			// be the most efficient approach (but still more than
			// fast enough, we typically only nned to adjust a few
			// bytes anyway). This has the advantage of being very
			// simple: it can easily handle gaps that wrap around
			// from the end to the beginning of the track and it
			// can handle the case that after decreasing a gap with
			// a few bytes another gaps becomes the largest gap.
			int delta;
			if (trackSize > v.size()) {
				delta = 1;
				v.insert(v.begin() + mid, v[mid]);
			} else {
				delta = -1;
				v.erase(v.begin() + mid);
			}

			// After inserting/deleting byte(s), we need to adjust
			// the IDAM table.
			for (int i = 0; i < 64; ++i) {
				int t = v[2 * i + 0] + 256 * v[2 * i + 1];
				if (t == 0) break;
				if ((t & 0x3fff) > mid) t += delta;
				v[2 * i + 0] = t % 256;
				v[2 * i + 1] = t / 256;
			}
		}

		// Write track to DMK file.
		if (fwrite(v.data(), v.size(), 1, file) != 1) {
			fprintf(stderr, "Error writing out.dmk.\n");
			exit(1);
		}
	}

	if (fclose(file)) {
		fprintf(stderr, "Error closing out.dmk.\n");
		exit(1);
	}

	printf("Successfully wrote out.dmk.\n");
	exit(0);
}
