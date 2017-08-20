/*
	Copyleft 2017
	NataliaPC aka @ishwin74
*/
#ifndef UINT24_HH
#define UINT24_HH

#include <cstdint>


#ifndef uint24_t
struct uint24_t {
	uint32_t value : 24;
	uint24_t() { value = 0; }
	operator int() {
		return value;
	}
	uint24_t(unsigned i) {
		value = i & 0xffffff;
	}
} __attribute__((packed));
#endif

#ifndef int24_t
struct int24_t {
	int32_t value : 24;
	int24_t() { value = 0; }
	int24_t(signed i) {
		value = (i & 0x7fffff);
		if (i<0) value = -value;
	}
} __attribute__((packed));
#endif


#endif
