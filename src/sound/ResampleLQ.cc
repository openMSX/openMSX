// $Id: ResampleLQ.cc 6476 2007-05-14 18:12:29Z m9710797 $

#include "ResampleLQ.hh"
#include "Resample.hh"
#include "aligned.hh"
#include <cassert>
#include <cstring>

namespace openmsx {

static const int BUFSIZE = 16384;
ALIGNED(static int bufferInt[BUFSIZE + 4], 16);

////

template<unsigned CHANNELS>
std::auto_ptr<ResampleLQ<CHANNELS> > ResampleLQ<CHANNELS>::create(
	Resample& input, double ratio)
{
	std::auto_ptr<ResampleLQ<CHANNELS> > result;
	if (ratio < 1.0) {
		result.reset(new ResampleLQUp  <CHANNELS>(input, ratio));
	} else {
		result.reset(new ResampleLQDown<CHANNELS>(input, ratio));
	}
	return result;
}

template <unsigned CHANNELS>
ResampleLQ<CHANNELS>::ResampleLQ(Resample& input_, double ratio)
	: input(input_), pos(0), step(ratio)
{
	for (unsigned j = 0; j < CHANNELS; ++j) {
		lastInput[j] = 0;
	}
}

template <unsigned CHANNELS>
bool ResampleLQ<CHANNELS>::fetchData(unsigned num)
{
	Pos end = pos + step * num;
	int numInput = end.toInt();
	assert((numInput + 1) <= BUFSIZE);
	int* buffer = &bufferInt[4 - CHANNELS];
	assert((long(&buffer[1 * CHANNELS]) & 15) == 0);

	if (!input.generateInput(&buffer[1 * CHANNELS], numInput)) {
		int last = 0;
		for (unsigned j = 0; j < CHANNELS; ++j) {
			last |= lastInput[j];
		}
		if (last == 0) {
			return false;
		}
		memset(&buffer[CHANNELS], 0, numInput * CHANNELS * sizeof(int));
	}
	for (unsigned j = 0; j < CHANNELS; ++j) {
		buffer[j] = lastInput[j];
		lastInput[j] = buffer[numInput * CHANNELS + j];
	}
	return true;
}

////

template <unsigned CHANNELS>
ResampleLQUp<CHANNELS>::ResampleLQUp(Resample& input, double ratio)
	: ResampleLQ<CHANNELS>(input, ratio)
{
	assert(ratio < 1.0); // only upsampling
}

template <unsigned CHANNELS>
bool ResampleLQUp<CHANNELS>::generateOutput(int* __restrict dataOut, unsigned num)
{
	if (!this->fetchData(num)) return false;

	// this is currently only used to upsample cassette player sound,
	// sound quality is not so important here, so use 0-th order
	// interpolation (instead of 1st-order).
	int* buffer = &bufferInt[4 - CHANNELS];
	for (unsigned i = 0; i < num; ++i) {
		int p0 = this->pos.toInt();
		for (unsigned j = 0; j < CHANNELS; ++j) {
			dataOut[i * CHANNELS + j] = buffer[p0 * CHANNELS + j];
		}
		this->pos += this->step;
	}

	this->pos = this->pos.fract();
	return true;
}

////

template <unsigned CHANNELS>
ResampleLQDown<CHANNELS>::ResampleLQDown(Resample& input, double ratio)
	: ResampleLQ<CHANNELS>(input, ratio)
{
	assert(ratio > 1.0); // can only do downsampling
}

template <unsigned CHANNELS>
bool ResampleLQDown<CHANNELS>::generateOutput(int* __restrict dataOut, unsigned num)
{
	if (!this->fetchData(num)) return false;

	int* buffer = &bufferInt[4 - CHANNELS];
#ifdef __arm__
	if (CHANNELS == 1) {
		unsigned dummy;
		// This asm code is equivalent to the c++ code below (does
		// 1st order interpolation). It's still a bit slow, so we
		// use 0th order interpolation. Sound quality is still good
		// especially on portable devices with only medium quality
		// speakers.
		/*asm volatile (
		"0:\n\t"
			"mov	r7,%[p],LSR #16\n\t"
			"add	r7,%[buf],r7,LSL #2\n\t"
			"ldmia	r7,{r7,r8}\n\t"
			"sub	r8,r8,r7\n\t"
			"and	%[t],%[p],%[m]\n\t"
			"mul	%[t],r8,%[t]\n\t"
			"add	%[t],r7,%[t],ASR #16\n\t"
			"str	%[t],[%[out]],#4\n\t"
			"add	%[p],%[p],%[s]\n\t"

			"mov	r7,%[p],LSR #16\n\t"
			"add	r7,%[buf],r7,LSL #2\n\t"
			"ldmia	r7,{r7,r8}\n\t"
			"sub	r8,r8,r7\n\t"
			"and	%[t],%[p],%[m]\n\t"
			"mul	%[t],r8,%[t]\n\t"
			"add	%[t],r7,%[t],ASR #16\n\t"
			"str	%[t],[%[out]],#4\n\t"
			"add	%[p],%[p],%[s]\n\t"

			"mov	r7,%[p],LSR #16\n\t"
			"add	r7,%[buf],r7,LSL #2\n\t"
			"ldmia	r7,{r7,r8}\n\t"
			"sub	r8,r8,r7\n\t"
			"and	%[t],%[p],%[m]\n\t"
			"mul	%[t],r8,%[t]\n\t"
			"add	%[t],r7,%[t],ASR #16\n\t"
			"str	%[t],[%[out]],#4\n\t"
			"add	%[p],%[p],%[s]\n\t"

			"mov	r7,%[p],LSR #16\n\t"
			"add	r7,%[buf],r7,LSL #2\n\t"
			"ldmia	r7,{r7,r8}\n\t"
			"sub	r8,r8,r7\n\t"
			"and	%[t],%[p],%[m]\n\t"
			"mul	%[t],r8,%[t]\n\t"
			"add	%[t],r7,%[t],ASR #16\n\t"
			"str	%[t],[%[out]],#4\n\t"
			"add	%[p],%[p],%[s]\n\t"

			"subs	%[n],%[n],#4\n\t"
			"bgt	0b\n\t"
			: [p]   "=r"  (pos)
			, [t]   "=&r" (dummy)
			:       "[p]" (pos)
			, [buf] "r"   (buffer)
			, [out] "r"   (dataOut)
			, [s]   "r"   (step)
			, [n]   "r"   (num)
			, [m]   "r"   (0xFFFF)
			: "r7","r8"
		);*/

		// 0th order interpolation
		asm volatile (
		"0:\n\t"
			"mov	%[t],%[p],LSR #16\n\t"
			"ldr	%[t],[%[buf],%[t],LSL #2]\n\t"
			"str	%[t],[%[out]],#4\n\t"
			"add	%[p],%[p],%[s]\n\t"

			"mov	%[t],%[p],LSR #16\n\t"
			"ldr	%[t],[%[buf],%[t],LSL #2]\n\t"
			"str	%[t],[%[out]],#4\n\t"
			"add	%[p],%[p],%[s]\n\t"

			"mov	%[t],%[p],LSR #16\n\t"
			"ldr	%[t],[%[buf],%[t],LSL #2]\n\t"
			"str	%[t],[%[out]],#4\n\t"
			"add	%[p],%[p],%[s]\n\t"

			"mov	%[t],%[p],LSR #16\n\t"
			"ldr	%[t],[%[buf],%[t],LSL #2]\n\t"
			"str	%[t],[%[out]],#4\n\t"
			"add	%[p],%[p],%[s]\n\t"

			"subs	%[n],%[n],#4\n\t"
			"bgt	0b\n\t"
			: [p]   "=r"  (this->pos)
			, [t]   "=&r" (dummy)
			:       "[p]" (this->pos)
			, [buf] "r"   (buffer)
			, [out] "r"   (dataOut)
			, [s]   "r"   (this->step)
			, [n]   "r"   (num)
			: "r9"
		);
	} else {
#endif
		for (unsigned i = 0; i < num; ++i) {
			int p = this->pos.toInt();
			typename ResampleLQ<CHANNELS>::Pos fract = this->pos.fract();
			for (unsigned j = 0; j < CHANNELS; ++j) {
				int s0 = buffer[(p + 0) * CHANNELS + j];
				int s1 = buffer[(p + 1) * CHANNELS + j];
				int out = s0 + (fract * (s1 - s0)).toInt();
				dataOut[i * CHANNELS + j] = out;
			}
			this->pos += this->step;
		}
#ifdef __arm__
	}
#endif
	this->pos = this->pos.fract();
	return true;
}


// Force template instantiation.
template class ResampleLQ<1>;
template class ResampleLQ<2>;

} // namespace openmsx
