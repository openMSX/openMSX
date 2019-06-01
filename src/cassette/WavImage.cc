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
	: clock(EmuTime::zero)
{
	File file(filename);
	setSha1Sum(filePool.getSha1Sum(file));

	wav = WavData(std::move(file), DCFilter{});
	clock.setFreq(wav.getFreq());
}

int16_t WavImage::getSampleAt(EmuTime::param time)
{
	return wav.getSample(clock.getTicksTill(time));
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
