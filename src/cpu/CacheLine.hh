#ifndef CACHELINE_HH
#define CACHELINE_HH

namespace openmsx::CacheLine {

constexpr unsigned BITS = 8; // 256 bytes
constexpr unsigned SIZE = 1 << BITS;
constexpr unsigned NUM  = 0x10000 / SIZE;
constexpr unsigned LOW  = SIZE - 1;
constexpr unsigned HIGH = 0xFFFF - LOW;

} // namespace openmsx::CacheLine

#endif
