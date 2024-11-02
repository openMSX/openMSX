#include "ResampleBlip.hh"
#include "ResampledSoundDevice.hh"

#include "narrow.hh"
#include "one_of.hh"
#include "ranges.hh"
#include "small_buffer.hh"
#include "xrange.hh"

#include <array>
#include <cassert>

namespace openmsx {

template<unsigned CHANNELS>
ResampleBlip<CHANNELS>::ResampleBlip(
		ResampledSoundDevice& input_, const DynamicClock& hostClock_)
	: ResampleAlgo(input_)
	, hostClock(hostClock_)
	, step([&]{ // calculate 'hostClock.getFreq() / getEmuClock().getFreq()', but with less rounding errors
			uint64_t emuPeriod = input_.getEmuClock().getPeriod().length(); // unknown units
			uint64_t hostPeriod = hostClock.getPeriod().length(); // unknown units, but same as above
			return FP::roundRatioDown(narrow<unsigned>(emuPeriod),
			                          narrow<unsigned>(hostPeriod));
		}())
{
	ranges::fill(lastInput, 0.0f);
}

template<unsigned CHANNELS>
bool ResampleBlip<CHANNELS>::generateOutputImpl(float* dataOut, size_t hostNum,
                                                EmuTime::param time)
{
	auto& emuClk = getEmuClock();
	if (unsigned emuNum = emuClk.getTicksTill(time); emuNum > 0) {
		// 3 extra for padding, CHANNELS extra for sentinel
		// Clang will produce a link error if the length expression is put
		// inside the macro.
		const unsigned len = emuNum * CHANNELS + std::max(3u, CHANNELS);
		small_buffer<float, 8192> buf(uninitialized_tag{}, len); // typical ~5194 (PSG, samples=1024) but could be larger
		EmuTime emu1 = emuClk.getFastAdd(1); // time of 1st emu-sample
		assert(emu1 > hostClock.getTime());
		if (input.generateInput(buf.data(), emuNum)) {
			FP pos1;
			hostClock.getTicksTill(emu1, pos1);
			for (auto ch : xrange(CHANNELS)) {
				// In case of PSG (and to a lesser degree SCC) it happens
				// very often that two consecutive samples have the same
				// value. We can benefit from this by setting a sentinel
				// at the end of the buffer and move the end-of-loop test
				// into the 'samples differ' branch.
				assert(emuNum > 0);
				buf[CHANNELS * emuNum + ch] =
					buf[CHANNELS * (emuNum - 1) + ch] + 1.0f;
				FP pos = pos1;
				auto last = lastInput[ch]; // local var is slightly faster
				for (unsigned i = 0; /**/; ++i) {
					if (auto delta = buf[CHANNELS * i + ch] - last;
					    delta != 0) [[unlikely]] {
						if (i == emuNum) {
							break;
						}
						last = buf[CHANNELS * i + ch];
						blip[ch].addDelta(
							BlipBuffer::TimeIndex(pos),
							delta);
					}
					pos += step;
				}
				lastInput[ch] = last;
			}
		} else {
			// input all zero
			BlipBuffer::TimeIndex pos;
			hostClock.getTicksTill(emu1, pos);
			for (auto ch : xrange(CHANNELS)) {
				if (lastInput[ch] != 0.0f) {
					auto delta = -lastInput[ch];
					lastInput[ch] = 0.0f;
					blip[ch].addDelta(pos, delta);
				}
			}
		}
		emuClk += emuNum;
	}

	std::array<bool, CHANNELS> results;
	for (auto ch : xrange(CHANNELS)) {
		results[ch] = blip[ch].template readSamples<CHANNELS>(dataOut + ch, hostNum);
	}
	static_assert(CHANNELS == one_of(1u, 2u), "either mono or stereo");
	if constexpr (CHANNELS == 1) {
		return results[0];
	} else {
		if (results[0] == results[1]) {
			// Both muted or both unmuted
			return results[0];
		} else {
			// One channel muted, the other not.
			// We have to set the muted channel to all-zero.
			unsigned offset = results[0] ? 1 : 0;
			for (auto i : xrange(hostNum)) {
				dataOut[2 * i + offset] = 0.0f;
			}
			return true;
		}
	}
}

// Force template instantiation.
template class ResampleBlip<1>;
template class ResampleBlip<2>;

} // namespace openmsx
