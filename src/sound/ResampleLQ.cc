#include "ResampleLQ.hh"
#include "ResampledSoundDevice.hh"
#include "likely.hh"
#include <cassert>
#include <cstring>
#include <memory>
#include <vector>

namespace openmsx {

// 16-byte aligned buffer of ints (shared among all instances of this resampler)
static std::vector<int> bufferStorage; // (possibly) unaligned storage
static unsigned bufferSize = 0; // usable buffer size (aligned portion)
static int* bufferInt = nullptr; // pointer to aligned sub-buffer

////

template<unsigned CHANNELS>
std::unique_ptr<ResampleLQ<CHANNELS>> ResampleLQ<CHANNELS>::create(
		ResampledSoundDevice& input,
		const DynamicClock& hostClock, unsigned emuSampleRate)
{
	std::unique_ptr<ResampleLQ<CHANNELS>> result;
	unsigned hostSampleRate = hostClock.getFreq();
	if (emuSampleRate < hostSampleRate) {
		result = std::make_unique<ResampleLQUp  <CHANNELS>>(
			input, hostClock, emuSampleRate);
	} else {
		result = std::make_unique<ResampleLQDown<CHANNELS>>(
			input, hostClock, emuSampleRate);
	}
	return result;
}

template <unsigned CHANNELS>
ResampleLQ<CHANNELS>::ResampleLQ(
		ResampledSoundDevice& input_,
		const DynamicClock& hostClock_, unsigned emuSampleRate)
	: input(input_)
	, hostClock(hostClock_)
	, emuClock(hostClock.getTime(), emuSampleRate)
	, step(FP::roundRatioDown(emuSampleRate, hostClock.getFreq()))
{
	for (auto& l : lastInput) l = 0;
}

template <unsigned CHANNELS>
bool ResampleLQ<CHANNELS>::fetchData(EmuTime::param time, unsigned& valid)
{
	unsigned emuNum = emuClock.getTicksTill(time);
	valid = 2 + emuNum;

	unsigned required = emuNum + 4;
	if (unlikely(required > bufferSize)) {
		// grow buffer (3 extra to be able to align)
		bufferStorage.resize(required + 3);
		// align at 16-byte boundary
		auto p = reinterpret_cast<uintptr_t>(bufferStorage.data());
		bufferInt = reinterpret_cast<int*>((p + 15) & ~15);
		// calculate actual usable size (the aligned portion)
		bufferSize = (bufferStorage.data() + bufferStorage.size()) - bufferInt;
		assert(bufferSize >= required);
	}

	emuClock += emuNum;
	assert(emuClock.getTime() <= time);
	assert(emuClock.getFastAdd(1) > time);

	int* buffer = &bufferInt[4 - 2 * CHANNELS];
	assert((uintptr_t(&buffer[2 * CHANNELS]) & 15) == 0);

	if (!input.generateInput(&buffer[2 * CHANNELS], emuNum)) {
		// New input is all zero
		int last = 0;
		for (auto& l : lastInput) last |= l;
		if (last == 0) {
			// Old input was also all zero, then the resampled
			// output will be all zero as well.
			return false;
		}
		memset(&buffer[CHANNELS], 0, emuNum * CHANNELS * sizeof(int));
	}
	for (unsigned j = 0; j < 2 * CHANNELS; ++j) {
		buffer[j] = lastInput[j];
		lastInput[j] = buffer[emuNum * CHANNELS + j];
	}
	return true;
}

////

template <unsigned CHANNELS>
ResampleLQUp<CHANNELS>::ResampleLQUp(
		ResampledSoundDevice& input_,
		const DynamicClock& hostClock_, unsigned emuSampleRate)
	: ResampleLQ<CHANNELS>(input_, hostClock_, emuSampleRate)
{
	assert(emuSampleRate < hostClock_.getFreq()); // only upsampling
}

template <unsigned CHANNELS>
bool ResampleLQUp<CHANNELS>::generateOutput(
	int* __restrict dataOut, unsigned hostNum, EmuTime::param time)
{
	EmuTime host1 = this->hostClock.getFastAdd(1);
	assert(host1 > this->emuClock.getTime());
	FP pos;
	this->emuClock.getTicksTill(host1, pos);
	assert(pos.toInt() < 2);

	unsigned valid; // only indices smaller than this number are valid
	if (!this->fetchData(time, valid)) return false;

	// this is currently only used to upsample cassette player sound,
	// sound quality is not so important here, so use 0-th order
	// interpolation (instead of 1st-order).
	int* buffer = &bufferInt[4 - 2 * CHANNELS];
	for (unsigned i = 0; i < hostNum; ++i) {
		unsigned p = pos.toInt();
		assert(p < valid);
		for (unsigned j = 0; j < CHANNELS; ++j) {
			dataOut[i * CHANNELS + j] = buffer[p * CHANNELS + j];
		}
		pos += this->step;
	}

	return true;
}

////

template <unsigned CHANNELS>
ResampleLQDown<CHANNELS>::ResampleLQDown(
		ResampledSoundDevice& input_,
		const DynamicClock& hostClock_, unsigned emuSampleRate)
	: ResampleLQ<CHANNELS>(input_, hostClock_, emuSampleRate)
{
	assert(emuSampleRate > hostClock_.getFreq()); // can only do downsampling
}

template <unsigned CHANNELS>
bool ResampleLQDown<CHANNELS>::generateOutput(
	int* __restrict dataOut, unsigned hostNum, EmuTime::param time)
{
	EmuTime host1 = this->hostClock.getFastAdd(1);
	assert(host1 > this->emuClock.getTime());
	FP pos;
	this->emuClock.getTicksTill(host1, pos);

	unsigned valid;
	if (!this->fetchData(time, valid)) return false;

	int* buffer = &bufferInt[4 - 2 * CHANNELS];
	for (unsigned i = 0; i < hostNum; ++i) {
		unsigned p = pos.toInt();
		assert((p + 1) < valid);
		FP fract = pos.fract();
		for (unsigned j = 0; j < CHANNELS; ++j) {
			int s0 = buffer[(p + 0) * CHANNELS + j];
			int s1 = buffer[(p + 1) * CHANNELS + j];
			int out = s0 + (fract * (s1 - s0)).toInt();
			dataOut[i * CHANNELS + j] = out;
		}
		pos += this->step;
	}
	return true;
}


// Force template instantiation.
template class ResampleLQ<1>;
template class ResampleLQ<2>;

} // namespace openmsx
