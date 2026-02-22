#ifndef PAPER_HH
#define PAPER_HH

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace openmsx {

/** Paper/Page rendering for printer emulation. */
class Paper
{
public:
	static constexpr std::string_view PRINT_DIR = "prints";
	static constexpr std::string_view PRINT_EXTENSION = ".png";

public:
	Paper(unsigned x, unsigned y, double dotSizeX, double dotSizeY);

	[[nodiscard]] std::string save() const;
	void setDotSize(double sizeX, double sizeY);
	void plot(double x, double y);

private:
	uint8_t& dot(unsigned x, unsigned y);

private:
	std::vector<uint8_t> buf;
	std::vector<int> table;

	double radiusX;
	double radiusY;
	int radius16;

	unsigned sizeX;
	unsigned sizeY;
};

} // namespace openmsx

#endif
