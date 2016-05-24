#include "WavImage.hh"
#include "LocalFileReference.hh"
#include "File.hh"
#include "FilePool.hh"
#include "Math.hh"
#include "memory.hh"
#include "xrange.hh"

#include <iostream>

namespace openmsx {

// In general a 1st order IIR filter looks like this:
//   y[n] = a0 * x[n]  +  a1 * x[n-1]  +  b1 * y[n-1]
//
// with
//  x[n] the n'th input  sample
//  y[n] the n'th output sample
//  a0, a1, b1 fixed coefficients
//
// For a low pass filter we have:
//   a0 = 1 - x
//   a1 = 0
//   b1 = x
//
// For a high pass filter we have:
//   a0 =  (1 + x) / 2
//   a1 = -(1 + x) / 2
//   b1 = x
//
// with
//   x = exp(-2 * pi * cut-off-freq / sample-freq)
//
//
// The class LowPassFilter and HighPassFilter below implement these filters but
// they take advantage of the special structure of the coefficients.

static const float tau = 2.0f * float(M_PI);

class LowPassFilter
{
public:
	LowPassFilter(float fc, float fs)
		: b(expf(-tau * fc / fs))
		, a(1.0f - b) {}

	float operator()(float x0)
	{
		y = a * x0 + b * y;
		return y;
	}

private:
	// coefficients
	const float b, a;
	// state
	float y = 0.0f;
};

class HighPassFilter
{
public:
	HighPassFilter(float fc, float fs)
		: b(expf(-tau * fc / fs))
		, a(0.5f + b * 0.5f) {}

	float operator()(float x0)
	{
		y = a * (x0 - x) + b * y;
		x = x0;
		return y;
	}

private:
	// coefficients
	const float b, a;
	// state
	float x = 0.0f, y = 0.0f;
};

static void bandPassFilter(float sampleFreq, short* buf, size_t n)
{
	// Pass band: 178Hz - 11379Hz
	// This is only a first order (low+high pass) filter, so frequencies
	// outside this band are also 'somewhat' coming through. However this
	// is roughly the same crude filtering as what the real hardware does.
	//
	// Filter reverse engineered from the circuit diagram in the
	// SonyHB-G900P service manual.
	//
	// TODO it would be very nice if someone could verify this filter (the
	// reverse engineering step). Analog electronics are not my strong
	// side, so it's not impossible I made a mistake.

	LowPassFilter  lpf(11379.f, sampleFreq);
	HighPassFilter hpf(  178.f, sampleFreq);

	for (auto i : xrange(n)) {
		buf[i] = Math::clipIntToShort(lrintf(hpf(lpf(buf[i]))));
	}
}

// Note: type detection not implemented yet for WAV images
WavImage::WavImage(const Filename& filename, FilePool& filePool)
	: clock(EmuTime::zero)
	, schmittPos(-1), schmittVal(false)
{
	LocalFileReference localFile;
	{
		// File object must be destroyed before localFile is actually
		// used by an external API (see comments in LocalFileReference
		// for details).
		File file(filename);
		setSha1Sum(filePool.getSha1Sum(file));
		localFile = LocalFileReference(file);
	}
	wav = WavData(localFile.getFilename(), 16, 0);
	clock.setFreq(wav.getFreq());

	bool debug = false;
	auto* buf = static_cast<int16_t*>(wav.getData());
	auto size = wav.getSize();
	if (debug) {
		for (auto i : xrange(size)) {
			std::cout << buf[i] << ' ';
		}
		std::cout << '\n';
	}

	bandPassFilter(wav.getFreq(), buf, size);

	if (debug) {
		for (auto i : xrange(size)) {
			std::cout << buf[i] << ' ';
		}
		std::cout << '\n';
		for (auto i : xrange(size)) {
			std::cout << cassetteIn(i) << ' ';
		}
		std::cout << '\n';
	}
}

int16_t WavImage::getSample(unsigned pos) const
{
	if (pos < wav.getSize()) {
		auto* buf = static_cast<const int16_t*>(wav.getData());
		return buf[pos];
	}
	return 0;
}

int16_t WavImage::getSampleAt(EmuTime::param time)
{
	return getSample(clock.getTicksTill(time));
}

bool WavImage::cassetteIn(int pos1)
{
	static const int16_t LOW  = -4000; // TODO
	static const int16_t HIGH =  4000; // TODO

	auto v = getSample(pos1);

	// Schmitt trigger: below LOW or above HIGH treshold we immediately
	// know the result
	bool result; int pos, end;
	if (v < LOW)  { result = false; goto found; }
	if (v > HIGH) { result = true;  goto found; }

	// We're in the middle area. Search backwards till either
	//  - we found a value above/below treshold
	//  - we're at the cached earlier calculated position
	//  - we're at the start of the tape
	pos = pos1;
	end = (pos >= schmittPos) ? schmittPos : 0;
	while (pos > end) {
		auto v2 = getSample(--pos);
		if (v2 < LOW)  { result = false; goto found; }
		if (v2 > HIGH) { result = true;  goto found; }
	}
	if (pos == schmittPos) { result = schmittVal; goto found2; }
	result = false;

        // cache the result to speedup future calculations
found:	schmittVal = result;
found2:	schmittPos = pos1;
	return result;
}

bool WavImage::cassetteIn(EmuTime::param time)
{
	return cassetteIn(clock.getTicksTill(time));
}

EmuTime WavImage::getEndTime() const
{
	DynamicClock clk(clock);
	clk += wav.getSize();
	return clk.getTime();
}

unsigned WavImage::getFrequency() const
{
	return clock.getFreq();
}

void WavImage::fillBuffer(unsigned pos, int** bufs, unsigned num) const
{
	if (pos < wav.getSize()) {
		for (auto i : xrange(num)) {
			bufs[0][i] = getSample(pos + i);
		}
	} else {
		bufs[0] = nullptr;
	}
}

} // namespace openmsx
