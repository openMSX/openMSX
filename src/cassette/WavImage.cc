#include "WavImage.hh"
#include "File.hh"
#include "Filename.hh"
#include "FilePool.hh"
#include "Math.hh"
#include "xrange.hh"

namespace openmsx {

// DC-removal filter
//   y(n) = x(n) - x(n-1) + R * y(n-1)
// see comments in MSXMixer.cc for more details
class DCFilter {
public:
	void setFreq(unsigned sampleFreq) {
		const float cuttOffFreq = 800.0f; // trial-and-error
		R = 1.0f - ((float(2 * M_PI) * cuttOffFreq) / sampleFreq);
	}
	int16_t operator()(int16_t x) {
		float t1 = R * t0 + x;
		int16_t y = Math::clipIntToShort(t1 - t0);
		t0 = t1;
		return y;
	}
private:
	float R;
	float t0 = 0.0f;
};

// Note: type detection not implemented yet for WAV images
WavImage::WavImage(const Filename& filename, FilePool& filePool)
	: clock(EmuTime::zero())
{
	File file(filename);
	setSha1Sum(filePool.getSha1Sum(file));

	wav = WavData(std::move(file), DCFilter{});
	clock.setFreq(wav.getFreq());
}

int16_t WavImage::getSampleAt(EmuTime::param time)
{
	// The WAV file is typically sampled at 44kHz, but the MSX may sample
	// the signal at arbitrary moments in time. Initially we would simply
	// returned the closest older sample point (sample-and-hold
	// resampling). Now we perform cubic interpolation between the 4
	// surrounding sample points. Presumably this results in more accurately
	// timed zero-crossings of the signal.
	//
	// Thanks to 'p_gimeno' for figuring out that cubic resampling makes
	// the tape "Ingrid's back" sha1:9493e8851e9f173b67670a9a3de4645918ef436f
	// work in openMSX (with sample-and-hold it didn't work).
	auto [sample, x] = clock.getTicksTillAsIntFloat(time);
	float p[4] = {
		float(wav.getSample(unsigned(sample) - 1)), // intentional: underflow wraps to UINT_MAX
		float(wav.getSample(sample + 0)),
		float(wav.getSample(sample + 1)),
		float(wav.getSample(sample + 2))
	};
	return Math::clipIntToShort(int(Math::cubicHermite(p + 1, x)));
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

void WavImage::fillBuffer(unsigned pos, float** bufs, unsigned num) const
{
	if (pos < wav.getSize()) {
		for (auto i : xrange(num)) {
			bufs[0][i] = wav.getSample(pos + i);
		}
	} else {
		bufs[0] = nullptr;
	}
}

float WavImage::getAmplificationFactorImpl() const
{
	return 1.0f / 32768;
}

} // namespace openmsx
