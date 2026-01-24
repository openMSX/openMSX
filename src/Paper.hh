/** \file Paper.hh
 * Paper/Page rendering for printer and plotter emulation.
 * Extracted from Printer.hh for shared use.
 */

#ifndef PAPER_HH
#define PAPER_HH

#include "gl_vec.hh"
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace openmsx {

class Paper {
public:
  static constexpr std::string_view PRINT_DIR = "prints";
  static constexpr std::string_view PRINT_EXTENSION = ".png";

public:
  Paper(unsigned x, unsigned y, double dotSizeX, double dotSizeY);

  [[nodiscard]] std::string save(bool color = false) const;
  void setDotSize(double sizeX, double sizeY);
  void plot(double x, double y);
  void plotColor(double x, double y, uint8_t r, uint8_t g, uint8_t b);

  [[nodiscard]] unsigned getWidth() const { return sizeX; }
  [[nodiscard]] unsigned getHeight() const { return sizeY; }
  [[nodiscard]] gl::vec2 getSize() const {
    return {float(sizeX), float(sizeY)};
  }
  [[nodiscard]] std::span<const uint8_t> getRGBData() const { return colorBuf; }
  [[nodiscard]] uint64_t getGeneration() const { return generation; }

private:
  uint8_t &dot(unsigned x, unsigned y);

  std::vector<uint8_t> buf;
  std::vector<uint8_t> colorBuf; // RGB triples for color output
  std::vector<int> table;

  uint64_t generation = 0;

  double radiusX;
  double radiusY;
  int radius16;

  unsigned sizeX;
  unsigned sizeY;
};

} // namespace openmsx

#endif
