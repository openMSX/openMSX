#include "CRC16.hh"

namespace openmsx {

// Accelerator table to compute the CRC (upto) 64 bits at a time
// (total table size is 4kB)
constexpr CRC16Lut CRC16::tab;

} // namespace openmsx
