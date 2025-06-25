// Based on code written by:
//   (2017) NataliaPC aka @ishwin74
//   Under GPL License

#include "TsxImage.hh"
#include "TsxParser.hh"
#include "File.hh"
#include "FilePool.hh"
#include "Filename.hh"
#include "CliComm.hh"
#include "Clock.hh"
#include "MSXException.hh"
#include "xrange.hh"

namespace openmsx {

TsxImage::TsxImage(const Filename& filename, FilePool& filePool, CliComm& cliComm)
{
	File file(filename);
	try {
		TsxParser parser(file.mmap());

		// Move the parsed waveform here
		output = std::move(parser.stealOutput());

		// Translate the TsxReader-filetype to a CassetteImage-filetype
		if (auto type = parser.getFirstFileType()) {
			setFirstFileType([&] {
				switch (*type) {
				case TsxParser::FileType::ASCII:  return CassetteImage::ASCII;
				case TsxParser::FileType::BINARY: return CassetteImage::BINARY;
				case TsxParser::FileType::BASIC:  return CassetteImage::BASIC;
				default:                          return CassetteImage::UNKNOWN;
				}
			}(), filename);
		}

		// Print embedded messages
		for (const auto& msg : parser.getMessages()) {
			cliComm.printInfo(msg);
		}
	} catch (const std::string& msg) {
		throw MSXException(filename.getOriginal(), ": ", msg);
	}

	// Conversion successful, now calculate sha1sum
	setSha1Sum(filePool.getSha1Sum(file));
}

int16_t TsxImage::getSampleAt(EmuTime time) const
{
	static const Clock<TsxParser::OUTPUT_FREQUENCY> zero(EmuTime::zero());
	unsigned pos = zero.getTicksTill(time);
	return pos < output.size() ? output[pos] * 256 : 0;
}

EmuTime TsxImage::getEndTime() const
{
	Clock<TsxParser::OUTPUT_FREQUENCY> clk(EmuTime::zero());
	clk += unsigned(output.size());
	return clk.getTime();
}

unsigned TsxImage::getFrequency() const
{
	return TsxParser::OUTPUT_FREQUENCY;
}

void TsxImage::fillBuffer(unsigned pos, std::span<float*, 1> bufs, unsigned num) const
{
	size_t nbSamples = output.size();
	if (pos < nbSamples) {
		for (auto i : xrange(num)) {
			bufs[0][i] = (pos < nbSamples)
			           ? output[pos]
			           : 0;
			++pos;
		}
	} else {
		bufs[0] = nullptr;
	}
}

float TsxImage::getAmplificationFactorImpl() const
{
	return 1.0f / 128;
}

} // namespace openmsx
