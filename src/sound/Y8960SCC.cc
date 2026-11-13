// The chip's behaviour, and the measurements it rests on, are described at
// the top of SCC.cc. Only the parts that differ from it are commented here.

#include "Y8960SCC.hh"

#include "DeviceConfig.hh"

#include "cstd.hh"
#include "enumerate.hh"
#include "narrow.hh"
#include "outer.hh"
#include "serialize.hh"
#include "unreachable.hh"
#include "xrange.hh"

#include <algorithm>
#include <array>
#include <cassert>

namespace openmsx {

static constexpr auto INPUT_RATE = unsigned(cstd::round(3579545.0 / 32));

Y8960SCC::Y8960SCC(const std::string& name_, const DeviceConfig& config, EmuTime time)
	: ResampledSoundDevice(
		config.getMotherBoard(), name_, "Y8960 SCC", NUM_CHANNELS, INPUT_RATE, true)
	, debuggable(config.getMotherBoard(), getName())
	, panDebuggable(config.getMotherBoard(), getName())
	, deformTimer(time)
{
	// Make valgrind happy
	std::ranges::fill(orgPeriod, 0);

	powerUp(time);
	registerSound(config);
}

Y8960SCC::~Y8960SCC()
{
	unregisterSound();
}

void Y8960SCC::powerUp(EmuTime time)
{
	// Initialize ch_enable, deform (initialize this before period)
	reset(time);

	// Initialize waveforms (initialize before volumes)
	for (auto& w1 : wave) {
		std::ranges::fill(w1, ~0);
	}
	// Initialize volume (initialize this before period)
	for (auto i : xrange(5)) {
		setFreqVol(i + 10, 15, time);
	}
	// Actual initial value is difficult to measure, assume zero
	// (initialize before period)
	std::ranges::fill(pos, 0);

	// Initialize period (sets members orgPeriod, period, incr, count, out)
	for (auto i : xrange(2 * 5)) {
		setFreqVol(i, 0, time);
	}
}

void Y8960SCC::reset(EmuTime /*time*/)
{
	setDeformRegHelper(0);
	ch_enable = 0;
	for (auto ch : xrange(NUM_CHANNELS)) storePan(ch, 128);
}

void Y8960SCC::storePan(unsigned channel, uint8_t value)
{
	pan[channel] = value;
	gain[channel] = y8960::panGain(value);
}

void Y8960SCC::setPan(unsigned channel, uint8_t value, EmuTime time)
{
	if (channel >= NUM_CHANNELS) return;
	updateStream(time);
	storePan(channel, value);
}

uint8_t Y8960SCC::readMem(uint8_t addr, EmuTime time)
{
	// 0xE0..0xFF : deformation register
	if (addr >= 0xE0) {
		setDeformReg(0xFF, time);
	}
	return peekMem(addr, time);
}

uint8_t Y8960SCC::peekMem(uint8_t address, EmuTime time) const
{
	if (address < 0x80) {
		// 0x00..0x7F : read wave form 1..4
		return readWave(address >> 5, address, time);
	} else {
		// 0x80..0x9F : freq volume block, write only
		// 0xA0..0xDF : no function
		// 0xE0..0xFF : deformation register
		return 0xFF;
	}
}

uint8_t Y8960SCC::readWave(unsigned channel, unsigned address, EmuTime time) const
{
	if (!rotate[channel]) {
		return wave[channel][address & 0x1F];
	} else {
		unsigned ticks = deformTimer.getTicksTill(time);
		unsigned periodCh = ((channel == 3) &&
		                     ((deformValue & 0xC0) == 0x40))
		                  ? 4 : channel;
		unsigned shift = ticks / (period[periodCh] + 1);
		return wave[channel][(address + shift) & 0x1F];
	}
}

uint8_t Y8960SCC::getFreqVol(unsigned address) const
{
	address &= 0x0F;
	if (address < 0x0A) {
		// get frequency
		unsigned channel = address / 2;
		if (address & 1) {
			return narrow_cast<uint8_t>(orgPeriod[channel] >> 8);
		} else {
			return narrow_cast<uint8_t>(orgPeriod[channel] & 0xFF);
		}
	} else if (address < 0x0F) {
		// get volume
		return volume[address - 0xA];
	} else {
		// get enable-bits
		return ch_enable;
	}
}

void Y8960SCC::writeMem(uint8_t address, uint8_t value, EmuTime time)
{
	updateStream(time);

	if (address < 0x80) {
		// 0x00..0x7F : write wave form 1..4
		writeWave(address >> 5, address, value);
	} else if (address < 0xA0) {
		// 0x80..0x9F : freq volume block
		setFreqVol(address, value, time);
	} else if (address < 0xE0) {
		// 0xA0..0xDF : no function
	} else {
		// 0xE0..0xFF : deformation register
		setDeformReg(value, time);
	}
}

float Y8960SCC::getAmplificationFactorImpl() const
{
	return 1.0f / 128.0f;
}

static constexpr float adjust(int8_t wav, uint8_t vol)
{
	// The result is an integer value, but we store it as a float because
	// then we need fewer int->float conversion (compared to converting in
	// generateChannels()).
	return float((int(wav) * vol) >> 4);
}

void Y8960SCC::writeWave(unsigned channel, unsigned address, uint8_t value)
{
	assert(channel < 5);

	if (!readOnly[channel]) {
		unsigned p = address & 0x1F;
		auto sValue = narrow_cast<int8_t>(value);
		wave[channel][p] = sValue;
		volAdjustedWave[channel][p] = adjust(sValue, volume[channel]);
		if (channel == 3) {
			// copy waveform 4 -> waveform 5
			wave[4][p] = wave[3][p];
			volAdjustedWave[4][p] = adjust(sValue, volume[4]);
		}
	}
}

void Y8960SCC::setFreqVol(unsigned address, uint8_t value, EmuTime time)
{
	address &= 0x0F; // region is visible twice
	if (address < 0x0A) {
		// change frequency
		unsigned channel = address / 2;
		unsigned per =
			  (address & 1)
			? ((value & 0xF) << 8) | (orgPeriod[channel] & 0xFF)
			: (orgPeriod[channel] & 0xF00) | (value & 0xFF);
		orgPeriod[channel] = per;
		if (deformValue & 2) {
			// 8 bit frequency
			per &= 0xFF;
		} else if (deformValue & 1) {
			// 4 bit frequency
			per >>= 8;
		}
		period[channel] = per;
		incr[channel] = (per <= 8) ? 0 : 32;
		count[channel] = 0; // reset to begin of byte
		if (deformValue & 0x20) {
			pos[channel] = 0; // reset to begin of waveform
			// also 'rotation' mode (confirmed by test based on
			// Artag's SCC sample player)
			deformTimer.advance(time);
		}
		// after a freq change, update the output
		out[channel] = volAdjustedWave[channel][pos[channel]];
	} else if (address < 0x0F) {
		// change volume
		unsigned channel = address - 0x0A;
		volume[channel] = value & 0xF;
		for (auto i : xrange(32)) {
			volAdjustedWave[channel][i] =
				adjust(wave[channel][i], volume[channel]);
		}
	} else {
		// change enable-bits
		ch_enable = value;
	}
}

void Y8960SCC::setDeformReg(uint8_t value, EmuTime time)
{
	if (value == deformValue) {
		return;
	}
	deformTimer.advance(time);
	setDeformRegHelper(value);
}

void Y8960SCC::setDeformRegHelper(uint8_t value)
{
	deformValue = value;
	switch (value & 0xC0) {
	case 0x00:
		std::ranges::fill(rotate, false);
		std::ranges::fill(readOnly, false);
		break;
	case 0x40:
		std::ranges::fill(rotate, true);
		std::ranges::fill(readOnly, true);
		break;
	case 0x80:
		for (auto i : xrange(3)) {
			rotate[i] = false;
			readOnly[i] = false;
		}
		for (auto i : xrange(3, 5)) {
			rotate[i] = true;
			readOnly[i] = true;
		}
		break;
	case 0xC0:
		for (auto i : xrange(3)) {
			rotate[i] = true;
			readOnly[i] = true;
		}
		for (auto i : xrange(3, 5)) {
			rotate[i] = false;
			readOnly[i] = true;
		}
		break;
	default:
		UNREACHABLE;
	}
}

void Y8960SCC::generateChannels(std::span<float*> bufs, unsigned num)
{
	unsigned enable = ch_enable;
	for (unsigned i = 0; i < 5; ++i, enable >>= 1) {
		if ((enable & 1) && (volume[i] || (out[i] != 0.0f))) {
			auto out2 = out[i];
			unsigned count2 = count[i];
			unsigned pos2 = pos[i];
			unsigned incr2 = incr[i];
			unsigned period2 = period[i] + 1;
			const auto g = gain[i];
			for (auto j : xrange(num)) {
				bufs[i][2 * j + 0] += out2 * g.left;
				bufs[i][2 * j + 1] += out2 * g.right;
				count2 += incr2;
				// Note: only for very small periods
				//       this will take more than 1 iteration
				while (count2 >= period2) [[unlikely]] {
					count2 -= period2;
					pos2 = (pos2 + 1) % 32;
					out2 = volAdjustedWave[i][pos2];
				}
			}
			out[i] = out2;
			count[i] = count2;
			pos[i] = pos2;
		} else {
			bufs[i] = nullptr; // channel muted
			// Update phase counter.
			unsigned newCount = count[i] + num * incr[i];
			count[i] = newCount % (period[i] + 1);
			pos[i] = (pos[i] + newCount / (period[i] + 1)) % 32;
			// Channel stays off until next waveform index.
			out[i] = 0.0f;
		}
	}
}


// Debuggable

Y8960SCC::Debuggable::Debuggable(MSXMotherBoard& motherBoard_, const std::string& name_)
	: SimpleDebuggable(motherBoard_, name_ + " SCC",
	                   "SCC registers in SCC+ format", 0x100)
{
}

uint8_t Y8960SCC::Debuggable::read(unsigned address, EmuTime time)
{
	const auto& scc = OUTER(Y8960SCC, debuggable);
	if (address < 0xA0) {
		// read wave form 1..5
		return scc.readWave(address >> 5, address, time);
	} else if (address < 0xC0) {
		// freq volume block
		return scc.getFreqVol(address);
	} else if (address < 0xE0) {
		// peek deformation register
		return scc.deformValue;
	} else {
		return 0xFF;
	}
}

void Y8960SCC::Debuggable::write(unsigned address, uint8_t value, EmuTime time)
{
	auto& scc = OUTER(Y8960SCC, debuggable);
	if (address < 0xA0) {
		// read wave form 1..5
		scc.writeWave(address >> 5, address, value);
	} else if (address < 0xC0) {
		// freq volume block
		scc.setFreqVol(address, value, time);
	} else if (address < 0xE0) {
		// deformation register
		scc.setDeformReg(value, time);
	} else {
		// ignore
	}
}

Y8960SCC::PanDebuggable::PanDebuggable(MSXMotherBoard& motherBoard_, const std::string& name_)
	: SimpleDebuggable(motherBoard_, name_ + " pan",
	                   "Y8960 SCC pan registers, channel A-E", NUM_CHANNELS)
{
}

uint8_t Y8960SCC::PanDebuggable::read(unsigned address, EmuTime /*time*/)
{
	const auto& scc = OUTER(Y8960SCC, panDebuggable);
	return scc.pan[address];
}

void Y8960SCC::PanDebuggable::write(unsigned address, uint8_t value, EmuTime time)
{
	auto& scc = OUTER(Y8960SCC, panDebuggable);
	scc.setPan(address, value, time);
}


// version 1: the members of the plain SCC; RomY8960 used to hold one
// version 2: pan
template<typename Archive>
void Y8960SCC::serialize(Archive& ar, unsigned version)
{
	ar.serialize("period",      orgPeriod,
	             "volume",      volume,
	             "ch_enable",   ch_enable,
	             "deformTimer", deformTimer,
	             "deform",      deformValue);
	// multi-dimensional arrays are not directly support by the
	// serialization framework, maybe in the future. So for now
	// manually loop over the channels.
	std::array<char, 6> tag = {'w', 'a', 'v', 'e', 'X', 0};
	for (auto [channel, wv] : enumerate(wave)) {
		tag[4] = char('1' + channel);
		ar.serialize(tag.data(), wv); // signed char
	}

	if (ar.versionAtLeast(version, 2)) {
		ar.serialize("pan", pan);
	}

	if constexpr (Archive::IS_LOADER) {
		// recalculate volAdjustedWave
		for (auto channel : xrange(5)) {
			for (auto p : xrange(32)) {
				volAdjustedWave[channel][p] =
					adjust(wave[channel][p], volume[channel]);
			}
		}

		// recalculate rotate[5] and readOnly[5]
		setDeformRegHelper(deformValue);

		// recalculate incr[5] and period[5]
		//  this also (possibly) changes count[5], pos[5] and out[5]
		//  as an unwanted side-effect, so (de)serialize those later
		// Don't use current time, but instead use deformTimer, to
		// avoid changing the value of deformTimer.
		EmuTime time = deformTimer.getTime();
		for (auto channel : xrange(5)) {
			unsigned per = orgPeriod[channel];
			setFreqVol(2 * channel + 0, (per & 0x0FF) >> 0, time);
			setFreqVol(2 * channel + 1, (per & 0xF00) >> 8, time);
		}

		if (ar.versionBelow(version, 2)) {
			std::ranges::fill(pan, uint8_t(128));
		}
		for (auto ch : xrange(NUM_CHANNELS)) storePan(ch, pan[ch]);
	}

	// call to setFreqVol() modifies these variables, see above
	ar.serialize("count", count,
	             "pos",   pos,
	             "out",   out);
}
INSTANTIATE_SERIALIZE_METHODS(Y8960SCC);

} // namespace openmsx
