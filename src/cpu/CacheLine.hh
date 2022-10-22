#ifndef CACHELINE_HH
#define CACHELINE_HH

namespace openmsx::CacheLine {

inline constexpr unsigned BITS = 8; // 256 bytes
inline constexpr unsigned SIZE = 1 << BITS;
inline constexpr unsigned NUM  = 0x10000 / SIZE;
inline constexpr unsigned LOW  = SIZE - 1;
inline constexpr unsigned HIGH = 0xFFFF - LOW;

} // namespace openmsx::CacheLine

#endif
