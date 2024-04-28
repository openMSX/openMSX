#ifndef DMK_COMMON_HH
#define DMK_COMMON_HH

#include <array>
#include <cstdint>
#include <cstdio>
#include <memory>
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

struct FClose {
	void operator()(FILE* f) const { fclose(f); }
};
using FILE_t = std::unique_ptr<FILE, FClose>;

class File
{
public:
	File(const std::string& filename, const char* mode)
		: file(fopen(filename.c_str(), mode))
	{
		if (!file) {
			throw std::runtime_error("Couldn't open: " + filename);
		}
	}

	void read(void* data, int size)
	{
		if (fread(data, size, 1, file.get()) != 1) {
			throw std::runtime_error("Couldn't read file");
		}
	}

	void write(const void* data, int size)
	{
		if (fwrite(data, size, 1, file.get()) != 1) {
			throw std::runtime_error("Couldn't write file");
		}
	}

private:
	FILE_t file;
};


inline void updateCrc(uint16_t& crc, uint8_t val)
{
	for (int i = 8; i < 16; ++i) {
		crc = (crc << 1) ^ (((crc ^ (val << i)) & 0x8000) ? 0x1021 : 0);
	}
}

#endif
