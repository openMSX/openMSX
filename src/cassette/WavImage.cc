// $Id$

#include "WavImage.hh"
#include "WavData.hh"
#include "MSXException.hh"
#include "LocalFileReference.hh"
#include "Math.hh"

using std::string;

namespace openmsx {

// Note: type detection not implemented yet for WAV images
WavImage::WavImage(const string& fileName)
	: clock(EmuTime::zero)
{
	LocalFileReference file(fileName);
	wav.reset(new WavData(file.getFilename(), 16, 0));
	clock.setFreq(wav->getFreq());

	// calculate the average to subtract it later (simple DC filter)
	unsigned nbSamples = wav->getSize();
	if (nbSamples > 0) {
		long long total = 0;
		for (unsigned i = 0; i < nbSamples; ++i) {
			total += getSample(i);
		}
		average = short(total / nbSamples);
	} else {
		average = 0;
	}
}

WavImage::~WavImage()
{
}

int WavImage::getSample(unsigned pos) const
{
	if (pos < wav->getSize()) {
		const short* buf = static_cast<const short*>(wav->getData());
		return buf[pos];
	}
	return 0;
}

short WavImage::getSampleAt(const EmuTime& time)
{
	unsigned pos = clock.getTicksTill(time);
	return Math::clipIntToShort(getSample(pos) - average);
}

EmuTime WavImage::getEndTime() const
{
	DynamicClock clk(clock);
	clk += wav->getSize();
	return clk.getTime();
}

unsigned WavImage::getFrequency() const
{
	return clock.getFreq();
}

void WavImage::fillBuffer(unsigned pos, int** bufs, unsigned num) const
{
	if (pos < wav->getSize()) {
		for (unsigned i = 0; i < num; ++i) {
			bufs[0][i] = getSample(pos + i);
		}
	} else {
		bufs[0] = 0;
	}
}

} // namespace openmsx
