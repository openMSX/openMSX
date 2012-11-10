// $Id$

#include "WavImage.hh"
#include "WavData.hh"
#include "LocalFileReference.hh"
#include "File.hh"
#include "Math.hh"
#include "memory.hh"

namespace openmsx {

// Note: type detection not implemented yet for WAV images
WavImage::WavImage(const Filename& filename, FilePool& filePool)
	: clock(EmuTime::zero)
{
	std::unique_ptr<LocalFileReference> localFile;
	{
		// File object must be destroyed before localFile is actually
		// used by an external API (see comments in LocalFileReference
		// for details).
		File file(filename);
		file.setFilePool(filePool);
		setSha1Sum(file.getSha1Sum());
		localFile = make_unique<LocalFileReference>(file);
	}
	wav = make_unique<WavData>(localFile->getFilename(), 16, 0);
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

short WavImage::getSampleAt(EmuTime::param time)
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
		bufs[0] = nullptr;
	}
}

} // namespace openmsx
