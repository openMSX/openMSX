#include "CasImage.hh"
#include "File.hh"
#include "FilePool.hh"
#include "Filename.hh"
#include "CliComm.hh"
#include "MSXException.hh"
#include "narrow.hh"
#include "ranges.hh"
#include "stl.hh"
#include "xrange.hh"
#include <span>

static constexpr std::array<uint8_t, 10> ASCII_HEADER  = { 0xEA,0xEA,0xEA,0xEA,0xEA,0xEA,0xEA,0xEA,0xEA,0xEA };
static constexpr std::array<uint8_t, 10> BINARY_HEADER = { 0xD0,0xD0,0xD0,0xD0,0xD0,0xD0,0xD0,0xD0,0xD0,0xD0 };
static constexpr std::array<uint8_t, 10> BASIC_HEADER  = { 0xD3,0xD3,0xD3,0xD3,0xD3,0xD3,0xD3,0xD3,0xD3,0xD3 };
// parsing code assumes these all have the same size
static_assert(ASCII_HEADER.size() == BINARY_HEADER.size());
static_assert(ASCII_HEADER.size() == BASIC_HEADER.size());

namespace openmsx {

// We oversample the audio signal for better sound quality (especially in
// combination with the hq resampler). Without oversampling the audio output
// could contain portions like this:
//   -1, +1, -1, +1, -1, +1, ...
// So it contains a signal at the Nyquist frequency. The hq resampler contains
// a low-pass filter, and (all practically implementable) filters cut off a
// portion of the spectrum near the Nyquist freq. So this high freq signal was
// lost after the hq resampler. After oversampling, the signal looks like this:
//   -1, -1, -1, -1, +1, +1, +1, +1, -1, -1, -1, -1, ...
// So every sample repeated 4 times.
static constexpr unsigned AUDIO_OVERSAMPLE = 4;

static void append(std::vector<int8_t>& wave, size_t count, int8_t value)
{
	wave.insert(wave.end(), count, value);
}

static void writeSilence(std::vector<int8_t>& wave, unsigned s)
{
	append(wave, s, 0);
}

static bool compare(const uint8_t* p, std::span<const uint8_t> rhs)
{
	return ranges::equal(std::span{p, rhs.size()}, rhs);
}

namespace MSX_CAS {

// a higher baudrate doesn't work anymore, but it is unclear why, because 4600
// baud should work (known from Speedsave 4000 and Turbo 5000 programs).
// 3765 still works on a Toshiba HX-10 and Philips NMS 8250, but not on a
// Panasonic FS-A1WSX, on which 3763 is the max. National CF-2000 has 3762 as
// the max. Let's take 3760 then as a safe value.
// UPDATE: that seems to break RUN"CAS:" type of programs. 3744 seems to work
// for those as well (we don't understand why yet)
static constexpr unsigned BAUDRATE = 3744;
static constexpr unsigned OUTPUT_FREQUENCY = 4 * BAUDRATE; // 4 samples per bit

// number of output bytes for silent parts
static constexpr unsigned SHORT_SILENCE = OUTPUT_FREQUENCY * 1; // 1 second
static constexpr unsigned LONG_SILENCE  = OUTPUT_FREQUENCY * 2; // 2 seconds

// number of 1-bits for headers
static constexpr unsigned LONG_HEADER  = 16000 / 2;
static constexpr unsigned SHORT_HEADER =  4000 / 2;

// headers definitions
static constexpr std::array<uint8_t, 8> CAS_HEADER = { 0x1F,0xA6,0xDE,0xBA,0xCC,0x13,0x7D,0x74 };

static void write0(std::vector<int8_t>& wave)
{
	static constexpr std::array<int8_t, 4> chunk{127, 127, -127, -127};
	::append(wave, chunk);
}
static void write1(std::vector<int8_t>& wave)
{
	static constexpr std::array<int8_t, 4> chunk{127, -127, 127, -127};
	::append(wave, chunk);
}

static void writeHeader(std::vector<int8_t>& wave, unsigned s)
{
	repeat(s, [&] { write1(wave); });
}

static void writeByte(std::vector<int8_t>& wave, uint8_t b)
{
	// one start bit
	write0(wave);
	// eight data bits
	for (auto i : xrange(8)) {
		if (b & (1 << i)) {
			write1(wave);
		} else {
			write0(wave);
		}
	}
	// two stop bits
	write1(wave);
	write1(wave);
}

// write data until a header is detected
static bool writeData(std::vector<int8_t>& wave, std::span<const uint8_t> cas, size_t& pos)
{
	bool eof = false;
	while ((pos + CAS_HEADER.size()) <= cas.size()) {
		if (compare(&cas[pos], CAS_HEADER)) {
			return eof;
		}
		writeByte(wave, cas[pos]);
		if (cas[pos] == 0x1A) {
			eof = true;
		}
		pos++;
	}
	while (pos < cas.size()) {
		writeByte(wave, cas[pos++]);
	}
	return false;
}

static CasImage::Data convert(std::span<const uint8_t> cas, const std::string& filename, CliComm& cliComm,
                              CassetteImage::FileType& firstFileType)
{
	CasImage::Data data;
	data.frequency = OUTPUT_FREQUENCY;
	auto& wave = data.wave;

	// search for a header in the .cas file
	bool issueWarning = false;
	bool headerFound = false;
	bool firstFile = true;
	size_t pos = 0;
	while ((pos + CAS_HEADER.size()) <= cas.size()) {
		if (compare(&cas[pos], CAS_HEADER)) {
			// it probably works fine if a long header is used for every
			// header but since the msx bios makes a distinction between
			// them, we do also (hence a lot of code).
			headerFound = true;
			pos += CAS_HEADER.size();
			writeSilence(wave, LONG_SILENCE);
			writeHeader(wave, LONG_HEADER);
			if ((pos + ASCII_HEADER.size()) <= cas.size()) {
				// determine file type
				auto type = [&] {
					if (compare(&cas[pos], ASCII_HEADER)) {
						return CassetteImage::ASCII;
					} else if (compare(&cas[pos], BINARY_HEADER)) {
						return CassetteImage::BINARY;
					} else if (compare(&cas[pos], BASIC_HEADER)) {
						return CassetteImage::BASIC;
					} else {
						return CassetteImage::UNKNOWN;
					}
				}();
				if (firstFile) firstFileType = type;
				switch (type) {
					case CassetteImage::ASCII:
						writeData(wave, cas, pos);
						do {
							pos += CAS_HEADER.size();
							writeSilence(wave, SHORT_SILENCE);
							writeHeader(wave, SHORT_HEADER);
							bool eof = writeData(wave, cas, pos);
							if (eof) break;
						} while ((pos + CAS_HEADER.size()) <= cas.size());
						break;
					case CassetteImage::BINARY:
					case CassetteImage::BASIC:
						writeData(wave, cas, pos);
						writeSilence(wave, SHORT_SILENCE);
						writeHeader(wave, SHORT_HEADER);
						pos += CAS_HEADER.size();
						writeData(wave, cas, pos);
						break;
					default:
						// unknown file type: using long header
						writeData(wave, cas, pos);
						break;
				}
			} else {
				// unknown file type: using long header
				writeData(wave, cas, pos);
			}
			firstFile = false;
		} else {
			// skipping unhandled data, shouldn't occur in normal cas file
			pos++;
			issueWarning = true;
		}
	}
	if (!headerFound) {
		throw MSXException(filename, ": not a valid CAS image");
	}
	if (issueWarning) {
		 cliComm.printWarning("Skipped unhandled data in ", filename);
	}

	return data;
}

} // namespace MSX_CAS

namespace SVI_CAS {

static constexpr std::array<uint8_t, 17> header = {
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	0x7f,
};

static void writeBit(std::vector<int8_t>& wave, bool bit)
{
	size_t count = bit ? 1 : 2;
	append(wave, count,  127);
	append(wave, count, -127);
}

static void writeByte(std::vector<int8_t>& wave, uint8_t byte)
{
	for (int i = 7; i >= 0; --i) {
		writeBit(wave, (byte >> i) & 1);
	}
}

static void processBlock(std::span<const uint8_t> subBuf, std::vector<int8_t>& wave)
{
	writeSilence(wave, 1200);
	writeBit(wave, true);
	repeat(199, [&] { writeByte(wave, 0x55); });
	writeByte(wave, 0x7f);
	for (uint8_t val : subBuf) {
		writeBit(wave, false);
		writeByte(wave, val);
	}
}

static CasImage::Data convert(std::span<const uint8_t> cas, CassetteImage::FileType& firstFileType)
{
	CasImage::Data data;
	data.frequency = 4800;

	if (cas.size() >= (header.size() + ASCII_HEADER.size())) {
		if (compare(&cas[header.size()], ASCII_HEADER)) {
			firstFileType = CassetteImage::ASCII;
		} else if (compare(&cas[header.size()], BINARY_HEADER)) {
			firstFileType = CassetteImage::BINARY;
		} else if (compare(&cas[header.size()], BASIC_HEADER)) {
			firstFileType = CassetteImage::BASIC;
		}
	}

	auto prevHeader = cas.begin() + header.size();
	while (true) {
		auto nextHeader = std::search(prevHeader, cas.end(),
		                              header.begin(), header.end());
		// Workaround clang-13/libc++ bug
		//processBlock(std::span(prevHeader, nextHeader), data.wave);
		processBlock(std::span(&*prevHeader, nextHeader - prevHeader), data.wave);
		if (nextHeader == cas.end()) break;
		prevHeader = nextHeader + header.size();
	}
	return data;
}

} // namespace SVI_CAS

CasImage::Data CasImage::init(const Filename& filename, FilePool& filePool, CliComm& cliComm)
{
	File file(filename);
	auto cas = file.mmap();

	auto fileType = CassetteImage::UNKNOWN;
	auto result = [&] {
		if ((cas.size() >= SVI_CAS::header.size()) &&
		    (compare(cas.data(), SVI_CAS::header))) {
			return SVI_CAS::convert(cas, fileType);
		} else {
			return MSX_CAS::convert(cas, filename.getOriginal(), cliComm, fileType);
		}
	}();
	setFirstFileType(fileType);

	// conversion successful, now calc sha1sum
	setSha1Sum(filePool.getSha1Sum(file));

	return result;
}

CasImage::CasImage(const Filename& filename, FilePool& filePool, CliComm& cliComm)
	: data(init(filename, filePool, cliComm))
{
}

int16_t CasImage::getSampleAt(EmuTime::param time) const
{
	EmuDuration d = time - EmuTime::zero();
	unsigned pos = d.getTicksAt(data.frequency);
	return narrow<int16_t>((pos < data.wave.size()) ? (data.wave[pos] * 256) : 0);
}

EmuTime CasImage::getEndTime() const
{
	EmuDuration d = EmuDuration::hz(data.frequency) * data.wave.size();
	return EmuTime::zero() + d;
}

unsigned CasImage::getFrequency() const
{
	return data.frequency * AUDIO_OVERSAMPLE;
}

void CasImage::fillBuffer(unsigned pos, std::span<float*, 1> bufs, unsigned num) const
{
	size_t nbSamples = data.wave.size();
	if ((pos / AUDIO_OVERSAMPLE) < nbSamples) {
		for (auto i : xrange(num)) {
			bufs[0][i] = ((pos / AUDIO_OVERSAMPLE) < nbSamples)
			           ? narrow_cast<float>(data.wave[pos / AUDIO_OVERSAMPLE])
			           : 0.0f;
			++pos;
		}
	} else {
		bufs[0] = nullptr;
	}
}

float CasImage::getAmplificationFactorImpl() const
{
	return 1.0f / 128.0f;
}

} // namespace openmsx
