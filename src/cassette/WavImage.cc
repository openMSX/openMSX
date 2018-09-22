#include "WavImage.hh"
#include "LocalFileReference.hh"
#include "File.hh"
#include "FilePool.hh"
#include "Math.hh"
#include "xrange.hh"

namespace openmsx {

// DC-removal filter
//   y(n) = x(n) - x(n-1) + R * y(n-1)
// see comments in MSXMixer.cc for more details
static void filter(float sampleFreq, int16_t* begin, int16_t* end)
{
	const float cuttOffFreq = 800.0f; // trial-and-error
	float R = 1.0f - ((float(2 * M_PI) * cuttOffFreq) / sampleFreq);

	float t0 = 0.0f;
	for (auto it = begin; it != end; ++it) {
		float t1 = R * t0 + *it;
		*it = Math::clipIntToShort(t1 - t0);
		t0 = t1;
	}
}

// Note: type detection not implemented yet for WAV images
WavImage::WavImage(const Filename& filename, FilePool& filePool)
	: clock(EmuTime::zero)
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

	auto* buf = static_cast<int16_t*>(wav.getData());
	auto* end = buf + wav.getSize();
	filter(wav.getFreq(), buf, end);
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
