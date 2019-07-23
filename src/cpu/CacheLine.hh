#ifndef CACHELINE_HH
#define CACHELINE_HH

namespace openmsx::CacheLine {

static const unsigned BITS = 8; // 256 bytes
static const unsigned SIZE = 1 << BITS;
static const unsigned NUM  = 0x10000 / SIZE;
static const unsigned LOW  = SIZE - 1;
static const unsigned HIGH = 0xFFFF - LOW;

} // namespace openmsx::CacheLine

#endif
