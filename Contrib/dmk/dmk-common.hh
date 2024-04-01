#ifndef DMK_COMMON_HH
#define DMK_COMMON_HH

#include <array>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>

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


inline void updateCrc(uint16_t& crc, uint8_t val)
{
	for (int i = 8; i < 16; ++i) {
		crc = (crc << 1) ^ (((crc ^ (val << i)) & 0x8000) ? 0x1021 : 0);
	}
}

#endif
