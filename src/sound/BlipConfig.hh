#ifndef BLIPCONFIG_HH
#define BLIPCONFIG_HH

// Number of bits in phase offset. Fewer than 6 bits (64 phase offsets) results
// in noticeable broadband noise when synthesizing high frequency square waves.
static const int BLIP_PHASE_BITS = 10;

// The input sample stream can only use this many bits out of the available 32
// bits. So 29 bits means the sample values must be in range [-256M, 256M].
static const int BLIP_SAMPLE_BITS = 29;

// Number of samples in a (pre-calculated) impulse-response wave-form.
static const int BLIP_IMPULSE_WIDTH = 16;

// Derived constants
static const int BLIP_RES = 1 << BLIP_PHASE_BITS;

#endif
