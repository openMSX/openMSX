//-----------------------------------------------------------------------------
//
// On Mon, 24 Feb 2003, Jon De Schrijder wrote:
//
// I've done some measurements with the scope on the output of the SCC.
// I didn't do timing tests, only amplitude checks:
//
// I know now for sure, the amplitude calculation works as follows:
//
// AmpOut=640+AmpA+AmpB+AmpC+AmpD+AmpE
//
// range AmpOut (11 bits positive number=SCC digital output): [+40...+1235]
//
// AmpA="((SampleValue*VolA) AND #7FF0) div 16"
// AmpB="((SampleValue*VolB) AND #7FF0) div 16"
// AmpC="((SampleValue*VolC) AND #7FF0) div 16"
// AmpD="((SampleValue*VolD) AND #7FF0) div 16"
// AmpE="((SampleValue*VolE) AND #7FF0) div 16"
//
// Setting the enable bit to zero, corresponds with VolX=0.
//
// SampleValue range [-128...+127]
// VolX range [0..15]
//
// Notes:
// * SampleValue*VolX is calculated (signed multiplication) and the lower 4
//   bits are dropped (both in case the value is positive or negative), before
//   the addition of the 5 values is done. This was tested by setting
//   SampleValue=+1 and VolX=15 of different channels. The resulting AmpOut=640,
//   indicating that the 4 lower bits were dropped *before* the addition.
//
//-----------------------------------------------------------------------------
//
// On Mon, 14 Apr 2003, Manuel Pazos wrote
//
// I have some info about SCC/SCC+ that I hope you find useful. It is about
// "Mode Setting Register", also called "Deformation Register" Here it goes:
//
//    bit0: 4 bits frequency (%XXXX00000000). Equivalent to
//          (normal frequency >> 8) bits0-7 are ignored
//    bit1: 8 bits frequency (%0000XXXXXXXX) bits8-11 are ignored
//    bit2:
//    bit3:
//    bit4:
//    bit5: wave data is played from beginning when frequency is changed
//    bit6: rotate all waves data. You can't write to them. Rotation speed
//          =3.58Mhz / (channel i frequency + 1)
//    bit7: rotate channel 4 wave data. You can't write to that channel
//          data.ONLY works in MegaROM SCC (not in SCC+)
//
// If bit7 and bit6 are set, only channel 1-3 wave data rotates . You can't
// write to ANY wave data. And there is a weird behaviour in this setting. It
// seems SCC sound is corrupted in anyway with MSX databus or so. Try to
// activate them (with proper waves, freqs, and vol.) and execute DIR command
// on DOS. You will hear "noise" This seems to be fixed in SCC+
//
// Reading Mode Setting Register, is equivalent to write #FF to it.
//
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
//
// Additions:
//   - Setting both bit0 and bit1 is equivalent to setting only bit1
//   - A rotation goes like this:
//       waveData[0:31] = waveData[1:31].waveData[0]
//   - Channel 4-5 rotation speed is set by channel 5 freq (channel 4 freq
//     is ignored for rotation)
//
// Also see this MRC thread:
//  http://www.msx.org/forumtopicl7875.html
//
//-----------------------------------------------------------------------------
//
// On Sat, 09 Sep 2005, NYYRIKKI wrote (MRC post)
//
// ...
//
// One important thing to know is that change of volume is not implemented
// immediately in SCC. Normally it is changed when next byte from sample memory
// is played, but writing value to frequency causes current byte to be started
// again. As in this example we write values very quickly to frequency registers
// the internal sample counter does not actually move at all.
//
// Third method is a variation of first method. As we don't know where SCC is
// playing, let's update the whole sample memory with one and same new value.
// To make sample rate not variable in low sample rates we first stop SCC from
// reading sample memory. This can be done by writing value less than 9 to
// frequency. Now we can update sample RAM so, that output does not change.
// After sample RAM has been updated, we start SCC internal counter so that
// value (where ever the counter was) is sent to output. This routine can be
// found below as example 3.
//
// ...
//
//
//
// Something completely different: the SCC+ is actually called SCC-I.
//-----------------------------------------------------------------------------

#include "SCC.hh"

#include "Clock.hh"
#include "DeviceConfig.hh"

#include "cstd.hh"
#include "enumerate.hh"
#include "outer.hh"
#include "serialize.hh"
#include "unreachable.hh"
#include "xrange.hh"

#include <algorithm>
#include <array>
#include <cmath>

namespace openmsx {

static constexpr auto INPUT_RATE = unsigned(cstd::round(3579545.0 / 32));

static constexpr auto calcDescription(SCC::Mode mode)
{
	return (mode == SCC::Mode::Real) ? static_string_view("Konami SCC")
	                                 : static_string_view("Konami SCC+");
}

SCC::SCC(const std::string& name_, const DeviceConfig& config,
         EmuTime time, Mode mode)
	: ResampledSoundDevice(
		config.getMotherBoard(), name_, calcDescription(mode), 5, INPUT_RATE, false)
	, debuggable(config.getMotherBoard(), getName())
	, currentMode(mode)
{
	// Make valgrind happy
	std::ranges::fill(orgPeriod, 0);

	powerUp(time);
	registerSound(config);
}

SCC::~SCC()
{
	unregisterSound();
}

void SCC::powerUp(EmuTime /*time*/)
{
	// Power on values, tested by enen (log from IRC #openmsx):
	//
	//  <enen>    wouter_: i did an scc poweron values test, deform=0,
	//            amplitude=full, channelEnable=0, period=under 8
	//    ...
	//  <wouter_> did you test the value of the waveforms as well?
	//    ...
	//  <enen>    filled with $FF, some bits cleared but that seems random

	// Initialize ch_enable, deform (initialize this before period)
	// (Not via reset(): that updates the stream, and this also runs
	// from the constructor, before the device is registered.)
	resetRegisters();

	// Initialize waveforms (initialize before volumes)
	for (auto& w1 : wave) {
		std::ranges::fill(w1, ~0);
	}
	std::ranges::fill(waveLatch, int8_t(~0));
	// Initialize volume (initialize this before period)
	for (auto i : xrange(5)) {
		setFreqVol(i + 10, 15);
	}
	// resetRegisters() above initialized pos, counter and out.

	// Initialize period (sets members orgPeriod, period, latchOutput,
	// counter, and out if the period allows it)
	for (auto i : xrange(2 * 5)) {
		setFreqVol(i, 0);
	}
}

void SCC::reset(EmuTime time)
{
	updateStream(time);
	resetRegisters();
}

void SCC::resetRegisters()
{
	if (currentMode != Mode::Real) {
		setMode(Mode::Compatible);
	}

	setDeformRegHelper(0);
	ch_enable = 0;
	// /RESET also clears the address counters, so every channel starts
	// its waveform from the top, and sets the frequency counters to all
	// ones: the first step comes 4096 cycles later, and only that reload
	// picks up the (unchanged) frequency register. The output latches go
	// quiet along with the enable bits.
	std::ranges::fill(pos, 0);
	std::ranges::fill(counter, 0xFFF);
	std::ranges::fill(out, 0.0f);
}

void SCC::setMode(Mode newMode)
{
	if (currentMode == Mode::Real) {
		assert(newMode == Mode::Real);
	} else {
		assert(newMode != Mode::Real);
	}
	currentMode = newMode;
}

uint8_t SCC::readMem(uint8_t addr, EmuTime time)
{
	// Deform-register locations:
	//   SCC_Real:       0xE0..0xFF
	//   SCC_Compatible: 0xC0..0xDF
	//   SCC_plusmode:   0xC0..0xDF
	if (((currentMode == Mode::Real) && (addr >= 0xE0)) ||
	    ((currentMode != Mode::Real) && (0xC0 <= addr) && (addr < 0xE0))) {
		updateStream(time);
		setDeformReg(0xFF);
	}
	// A read in 'rotation' mode returns the byte being played, so bring
	// the wave pointers up to date first.
	if (std::ranges::any_of(rotate, [](bool r) { return r; })) {
		updateStream(time);
	}
	return peekMem(addr, time);
}

uint8_t SCC::peekMem(uint8_t address, EmuTime /*time*/) const
{
	switch (currentMode) {
	case Mode::Real:
		if (address < 0x80) {
			// 0x00..0x7F : read wave form 1..4
			return readWave(address >> 5, address);
		} else {
			// 0x80..0x9F : freq volume block, write only
			// 0xA0..0xDF : no function
			// 0xE0..0xFF : deformation register
			return 0xFF;
		}
	case Mode::Compatible:
		if (address < 0x80) {
			// 0x00..0x7F : read wave form 1..4
			return readWave(address >> 5, address);
		} else if (address < 0xA0) {
			// 0x80..0x9F : freq volume block
			return 0xFF;
		} else if (address < 0xC0) {
			// 0xA0..0xBF : read wave form 5
			return readWave(4, address);
		} else {
			// 0xC0..0xDF : deformation register
			// 0xE0..0xFF : no function
			return 0xFF;
		}
	case Mode::Plus:
		if (address < 0xA0) {
			// 0x00..0x9F : read wave form 1..5
			return readWave(address >> 5, address);
		} else {
			// 0xA0..0xBF : freq volume block
			// 0xC0..0xDF : deformation register
			// 0xE0..0xFF : no function
			return 0xFF;
		}
	default:
		UNREACHABLE;
	}
}

uint8_t SCC::readWave(unsigned channel, unsigned address) const
{
	if (!rotate[channel]) {
		return wave[channel][address & 0x1F];
	}
	// 'Rotation'. With deformation bit 6 set the wave RAM's address mux
	// stays on the play pointer during a CPU access (the die schematic's
	// CH1 RAM block: TEST_D6 forces the counter side of the mux and
	// disables the write strobe), so a read returns the byte being
	// played, whichever address was read. Read one address repeatedly and
	// the waveform passes by at the channel's rate, which is what was
	// always observed and described as the wave rotating.
	//
	// Channels 4 and 5 share one RAM whose address mux alternates between
	// their pointers under a 32-cycle sequencer. What that does exactly
	// during a CPU access in this mode is not settled (see the commit
	// message); this keeps the documented observation: with bit 6 the
	// shared RAM follows channel 5's pointer, with bit 7 channel 4's.
	unsigned ptr = ((channel == 3) && (currentMode != Mode::Plus) &&
	                ((deformValue & 0xC0) == 0x40)) ? 4 : channel;
	return wave[channel][pos[ptr]];
}


uint8_t SCC::getFreqVol(unsigned address) const
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

void SCC::writeMem(uint8_t address, uint8_t value, EmuTime time)
{
	updateStream(time);

	switch (currentMode) {
	case Mode::Real:
		if (address < 0x80) {
			// 0x00..0x7F : write wave form 1..4
			writeWave(address >> 5, address, value);
		} else if (address < 0xA0) {
			// 0x80..0x9F : freq volume block
			setFreqVol(address, value);
		} else if (address < 0xE0) {
			// 0xA0..0xDF : no function
		} else {
			// 0xE0..0xFF : deformation register
			setDeformReg(value);
		}
		break;
	case Mode::Compatible:
		if (address < 0x80) {
			// 0x00..0x7F : write wave form 1..4
			writeWave(address >> 5, address, value);
		} else if (address < 0xA0) {
			// 0x80..0x9F : freq volume block
			setFreqVol(address, value);
		} else if (address < 0xC0) {
			// 0xA0..0xBF : ignore write wave form 5
		} else if (address < 0xE0) {
			// 0xC0..0xDF : deformation register
			setDeformReg(value);
		} else {
			// 0xE0..0xFF : no function
		}
		break;
	case Mode::Plus:
		if (address < 0xA0) {
			// 0x00..0x9F : write wave form 1..5
			writeWave(address >> 5, address, value);
		} else if (address < 0xC0) {
			// 0xA0..0xBF : freq volume block
			setFreqVol(address, value);
		} else if (address < 0xE0) {
			// 0xC0..0xDF : deformation register
			setDeformReg(value);
		} else {
			// 0xE0..0xFF : no function
		}
		break;
	default:
		UNREACHABLE;
	}
}

float SCC::getAmplificationFactorImpl() const
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

void SCC::writeWave(unsigned channel, unsigned address, uint8_t value)
{
	// in Real mode channels 4 and 5 are one RAM, addressed as channel 4
	assert(channel < 5);
	assert((channel != 4) || (currentMode != Mode::Real));

	if (!readOnly[channel]) {
		unsigned p = address & 0x1F;
		auto sValue = narrow_cast<int8_t>(value);
		wave[channel][p] = sValue;
		volAdjustedWave[channel][p] = adjust(sValue, volume[channel]);
		if ((currentMode != Mode::Plus) && (channel == 3)) {
			// copy waveform 4 -> waveform 5
			wave[4][p] = wave[3][p];
			volAdjustedWave[4][p] = adjust(sValue, volume[4]);
		}
	}
}

// Recalculate the effective period, and whether the output latch is still
// being updated, from orgPeriod[] and the deformation register.
void SCC::updatePeriod(unsigned channel)
{
	unsigned per = orgPeriod[channel];
	if (deformValue & 2) {
		// 8 bit frequency
		per &= 0xFF;
	} else if (deformValue & 1) {
		// 4 bit frequency
		per >>= 8;
	}
	period[channel] = per;
	// The SCC computes each sample with a serial multiplier that needs 9
	// master clock cycles, one more than the period+1 cycles available
	// below period 8. Under that the multiplier is restarted before it
	// ever finishes and the output latch keeps its old value. The wave
	// pointer is driven by the same frequency counter and keeps running.
	latchOutput[channel] = per >= 8;
}

// Advance the frequency counter of a channel by the given number of master
// clock cycles, and return how often the wave pointer stepped.
//
// The counter is a chain of three 4-bit down counters loaded from the
// frequency register. Normally it acts as one 12-bit down counter that
// reloads, and steps the wave pointer, on the cycle it reaches zero.
// Deformation bit 1 takes the step from the low byte reaching zero instead,
// so the top nibble never moves. Bit 0 makes the top nibble count every
// cycle on its own, and the low byte just runs along (and wraps) until the
// reload. The counter itself is the same in every mode, which is what
// keeps this continuous across a change of the deformation register.
unsigned SCC::advanceCounter(unsigned channel, unsigned clocks)
{
	unsigned org = orgPeriod[channel];
	unsigned cnt = counter[channel];
	unsigned steps = 0;
	if (deformValue & 2) {
		// 8 bit frequency: step on the low byte
		unsigned c  = cnt >> 8;
		unsigned ba = cnt & 0xFF;
		if (clocks > ba) {
			clocks -= ba + 1;
			unsigned p = (org & 0xFF) + 1;
			steps = 1 + clocks / p;
			clocks %= p;
			c  = org >> 8;
			ba = org & 0xFF;
		}
		ba -= clocks;
		// bit 0 still makes the top nibble count every cycle, which
		// only shows once the mode is switched again
		if (deformValue & 1) c = (c - clocks) & 0xF;
		cnt = (c << 8) | ba;
	} else if (deformValue & 1) {
		// 4 bit frequency: step on the top nibble, which counts every
		// cycle, the low byte runs along
		unsigned c  = cnt >> 8;
		unsigned ba = cnt & 0xFF;
		if (clocks > c) {
			clocks -= c + 1;
			unsigned p = (org >> 8) + 1;
			steps = 1 + clocks / p;
			clocks %= p;
			c  = org >> 8;
			ba = org & 0xFF;
		}
		cnt = ((c - clocks) << 8) | ((ba - clocks) & 0xFF);
	} else {
		// 12 bit frequency
		if (clocks > cnt) {
			clocks -= cnt + 1;
			unsigned p = org + 1;
			steps = 1 + clocks / p;
			clocks %= p;
			cnt = org;
		}
		cnt -= clocks;
	}
	counter[channel] = cnt;
	return steps;
}

// The number of cycles the frequency counter still counts down before the
// next step, under the current deformation mode.
unsigned SCC::remaining(unsigned channel) const
{
	unsigned cnt = counter[channel];
	if (deformValue & 2) return cnt & 0xFF;
	if (deformValue & 1) return cnt >> 8;
	return cnt;
}

// Channels 4 and 5 of the SCC share one wave RAM, and don't read it the way
// channels 1 to 3 read theirs. A free-running 32-cycle sequencer gives the
// RAM channel 4's address for 16 cycles and channel 5's for the other 16,
// and halfway through each window captures the byte in that channel's
// latch. The multiplier reads bits from the latch, one per cycle, during
// the 8 cycles after a step. So it sees the byte the pointer was at when
// the latch was last captured, normally one position behind, and when a
// capture lands inside those 8 cycles the low bits come from the old byte
// and the high bits from the new one.
//
// The sequencer's period is exactly one of our samples, so within a sample
// each latch is captured at a fixed edge: channel 4's on the first edge
// after reset, channel 5's 16 later. Where that sits relative to the
// sample grid can't be known here, only that it is fixed.
bool SCC::isLatched(unsigned channel) const
{
	return (currentMode == Mode::Real) && (channel >= 3);
}

// One 32-cycle sample of a latched channel: advance the counter, the wave
// pointer and the latch. Returns the output at the end of the sample, or
// 'current' when there was no step.
float SCC::stepLatched(unsigned channel, float current)
{
	static constexpr std::array<unsigned, 2> CAPTURE_EDGE = {1, 17};
	unsigned capture = CAPTURE_EDGE[channel - 3];
	unsigned s = remaining(channel) + 1; // edge of the first step
	unsigned steps = advanceCounter(channel, 32);
	unsigned p = period[channel] + 1;
	auto& latch = waveLatch[channel - 3];
	const auto& ram = wave[channel];
	unsigned pos2 = pos[channel];
	bool captured = false;
	repeat(steps, [&] {
		if (!captured && (capture <= s)) {
			// on the same edge as the step it still sees the old
			// position, and the multiplier then reads the new latch
			latch = ram[pos2];
			captured = true;
		}
		pos2 = (pos2 + 1) % 32;
		int8_t b = latch;
		// the next capture, possibly in the following sample
		unsigned d = ((capture > s) ? capture : (capture + 32)) - s;
		if (d <= 7) {
			// inside the multiplier's window: bits 0..d-1 were
			// already read from the old byte
			auto mask = uint8_t((1 << d) - 1);
			b = narrow_cast<int8_t>((uint8_t(ram[pos2]) & ~mask) |
			                        (uint8_t(latch) & mask));
		}
		current = adjust(b, volume[channel]);
		s += p;
	});
	if (!captured) latch = ram[pos2];
	pos[channel] = pos2;
	return current;
}

// Advance a channel over 'num' samples without producing output.
void SCC::advanceBlock(unsigned channel, unsigned num)
{
	if (isLatched(channel)) {
		// only the last sample matters for the latch
		if (num > 1) {
			pos[channel] = (pos[channel] + advanceCounter(channel, (num - 1) * 32)) % 32;
		}
		(void)stepLatched(channel, 0.0f);
	} else {
		pos[channel] = (pos[channel] + advanceCounter(channel, num * 32)) % 32;
	}
}

void SCC::setFreqVol(unsigned address, uint8_t value)
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
		updatePeriod(channel);
		counter[channel] = orgPeriod[channel]; // reload, restart the byte
		if (deformValue & 0x20) {
			pos[channel] = 0; // reset to begin of waveform
		}
		// After a freq change the multiplier restarts, refreshing the
		// output with the current sample and the current volume -- but
		// only when the period is long enough for it to finish.
		if (latchOutput[channel]) {
			out[channel] = isLatched(channel)
				? adjust(waveLatch[channel - 3], volume[channel])
				: volAdjustedWave[channel][pos[channel]];
		}
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

void SCC::setDeformReg(uint8_t value)
{
	if (value == deformValue) {
		return;
	}
	setDeformRegHelper(value);
}

void SCC::setDeformRegHelper(uint8_t value)
{
	deformValue = value;
	// Bits 0 and 1 (4 bit / 8 bit frequency) are gates in the frequency
	// counter chain, so they change the pitch right away, without waiting
	// for a write to a frequency register.
	for (auto channel : xrange(5)) {
		updatePeriod(channel);
	}
	if (currentMode != Mode::Real) {
		value &= ~0x80;
	}
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

void SCC::generateChannels(std::span<float*> bufs, unsigned num)
{
	unsigned enable = ch_enable;
	for (unsigned i = 0; i < 5; ++i, enable >>= 1) {
		if (!(enable & 1) || (!volume[i] && (out[i] == 0.0f))) {
			bufs[i] = nullptr; // channel muted
			// The wave pointer keeps running.
			advanceBlock(i, num);
			// Channel stays off until next waveform index.
			out[i] = 0.0f;
		} else if (!latchOutput[i]) {
			// Period too short for the multiplier to finish, so the
			// output latch keeps its value. The wave pointer does
			// keep running.
			auto out2 = out[i];
			for (auto j : xrange(num)) {
				bufs[i][j] += out2;
			}
			advanceBlock(i, num);
		} else if (isLatched(i)) {
			auto out2 = out[i];
			for (auto j : xrange(num)) {
				bufs[i][j] += out2;
				out2 = stepLatched(i, out2);
			}
			out[i] = out2;
		} else {
			auto out2 = out[i];
			unsigned pos2 = pos[i];
			for (auto j : xrange(num)) {
				bufs[i][j] += out2;
				// Note: only for very small periods
				//       this steps more than once per sample
				if (unsigned steps = advanceCounter(i, 32); steps != 0) [[unlikely]] {
					pos2 = (pos2 + steps) % 32;
					out2 = volAdjustedWave[i][pos2];
				}
			}
			out[i] = out2;
			pos[i] = pos2;
		}
	}
}


// Debuggable

SCC::Debuggable::Debuggable(MSXMotherBoard& motherBoard_, const std::string& name_)
	: SimpleDebuggable(motherBoard_, name_ + " SCC",
	                   "SCC registers in SCC+ format", 0x100)
{
}

uint8_t SCC::Debuggable::read(unsigned address, EmuTime /*time*/)
{
	const auto& scc = OUTER(SCC, debuggable);
	if (address < 0xA0) {
		// read wave form 1..5
		return scc.readWave(address >> 5, address);
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

void SCC::Debuggable::write(unsigned address, uint8_t value, EmuTime time)
{
	auto& scc = OUTER(SCC, debuggable);
	scc.updateStream(time);
	if (address < 0xA0) {
		// write wave form 1..5
		unsigned channel = address >> 5;
		if ((channel == 4) && (scc.currentMode == Mode::Real)) {
			channel = 3; // one RAM for channels 4 and 5
		}
		scc.writeWave(channel, address, value);
	} else if (address < 0xC0) {
		// freq volume block
		scc.setFreqVol(address, value);
	} else if (address < 0xE0) {
		// deformation register
		scc.setDeformReg(value);
	} else {
		// ignore
	}
}


static constexpr auto chipModeInfo = std::to_array<enum_string<SCC::Mode>>({
	{ "Real",       SCC::Mode::Real       },
	{ "Compatible", SCC::Mode::Compatible },
	{ "Plus",       SCC::Mode::Plus   },
});
SERIALIZE_ENUM(SCC::Mode, chipModeInfo);

// version 1: initial version
// version 2: 'count', the number of cycles since the frequency counter was
//            last reloaded, replaced by 'counter', the frequency counter
//            itself. The former only made sense for one setting of the
//            deformation register.
// version 3: added 'waveLatch', the byte channels 4 and 5 last captured
//            from their shared wave RAM
// version 4: removed 'deformTimer', the reference for the old rotation model
template<typename Archive>
void SCC::serialize(Archive& ar, unsigned version)
{
	ar.serialize("mode",        currentMode,
	             "period",      orgPeriod,
	             "volume",      volume,
	             "ch_enable",   ch_enable);
	if (ar.versionBelow(version, 4)) {
		Clock<CLOCK_FREQ> deformTimer(EmuTime::zero()); // no longer used
		ar.serialize("deformTimer", deformTimer);
	}
	ar.serialize("deform", deformValue);
	// multi-dimensional arrays are not directly support by the
	// serialization framework, maybe in the future. So for now
	// manually loop over the channels.
	std::array<char, 6> tag = {'w', 'a', 'v', 'e', 'X', 0};
	for (auto [channel, wv] : enumerate(wave)) {
		tag[4] = char('1' + channel);
		ar.serialize(tag.data(), wv); // signed char
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

		// recalculate latchOutput[5] and period[5]
		//  this also (possibly) changes counter[5], pos[5] and out[5]
		//  as an unwanted side-effect, so (de)serialize those later
		for (auto channel : xrange(5)) {
			unsigned per = orgPeriod[channel];
			setFreqVol(2 * channel + 0, (per & 0x0FF) >> 0);
			setFreqVol(2 * channel + 1, (per & 0xF00) >> 8);
		}
	}

	// call to setFreqVol() modifies these variables, see above
	if (ar.versionAtLeast(version, 2)) {
		ar.serialize("counter", counter);
	} else {
		std::array<unsigned, 5> count; // cycles since the last reload
		ar.serialize("count", count);
		for (auto channel : xrange(5)) {
			unsigned org = orgPeriod[channel];
			unsigned n = std::min(count[channel], period[channel]);
			unsigned rem = period[channel] - n; // what is left to count
			counter[channel] = (deformValue & 2) ? ((org & 0xF00) | rem)
			                 : (deformValue & 1) ? ((rem << 8) | ((org - n) & 0xFF))
			                 : rem;
		}
	}
	ar.serialize("pos", pos,
	             "out", out); // note: changed int->float, but no need to bump serialize-version
	if (ar.versionAtLeast(version, 3)) {
		ar.serialize("waveLatch", waveLatch);
	} else if constexpr (Archive::IS_LOADER) {
		waveLatch = {wave[3][pos[3]], wave[4][pos[4]]};
	}
}
INSTANTIATE_SERIALIZE_METHODS(SCC);

} // namespace openmsx
