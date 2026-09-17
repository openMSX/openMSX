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
#include <ranges>

namespace openmsx {

static constexpr auto INPUT_RATE = unsigned(cstd::round(3579545.0 / 32));

// Every sample covers 32 master clock cycles. The shared wave RAM of channels
// 4 and 5 is captured into their latches at these edges of it, see isLatched().
static constexpr std::array<unsigned, 2> CAPTURE_EDGE = {1, 17};

// For how many cycles a CPU access holds a channel's RAM at the CPU's
// address: the address bus is decoded without /RD or /WR, so for the whole
// bus cycle.
static constexpr unsigned ACCESS_CYCLES = 3;

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
	for (auto& ev : events) {
		ev.reserve(64);
	}

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

	// Initialize ch_enable, deform, pointers, counters, output.
	// (Not via reset(): that updates the stream, and this also runs
	// from the constructor, before the device is registered.)
	resetRegisters();

	// Initialize waveforms
	for (auto& w1 : wave) {
		std::ranges::fill(w1, ~0);
	}
	std::ranges::fill(waveLatch, int8_t(~0));
	// Initialize volume
	for (auto i : xrange(5)) {
		setFreqVol(i + 10, 15);
	}
	// Initialize the frequency registers. The counters keep their reset
	// value: only a write reloads them.
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
	// quiet along with the enable bits, and the multipliers stop.
	std::ranges::fill(pos, 0);
	std::ranges::fill(counter, 0xFFF);
	std::ranges::fill(out, 0.0f);
	for (auto& m : multiplier) m = Multiplier{};
	for (auto& ev : events) ev.clear();
	std::ranges::fill(stealUntil, 0);
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

// Which channel's wave RAM window an address falls in, for reading or for
// writing, or -1.
int SCC::waveChannel(uint8_t address, bool write) const
{
	switch (currentMode) {
	case Mode::Real:
		// 0x00..0x7F : wave form 1..4 (4 and 5 share their RAM)
		return (address < 0x80) ? int(address >> 5) : -1;
	case Mode::Compatible:
		// 0x00..0x7F : wave form 1..4
		// 0xA0..0xBF : wave form 5, read only
		if (address < 0x80) return int(address >> 5);
		if (!write && (0xA0 <= address) && (address < 0xC0)) return 4;
		return -1;
	case Mode::Plus:
		// 0x00..0x9F : wave form 1..5
		return (address < 0xA0) ? int(address >> 5) : -1;
	default:
		UNREACHABLE;
	}
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
	if (int channel = waveChannel(addr, false); channel >= 0) {
		// The access takes the RAM's address away from the channel for
		// the bus cycle, see runSample(). And in rotation mode the byte
		// returned depends on the wave pointer, so bring it up to date.
		updateStream(time);
		queueEvent(unsigned(channel), {accessCycle(time), Event::READ, uint8_t(addr & 0x1F), 0});
	}
	return peekMem(addr, time);
}

uint8_t SCC::peekMem(uint8_t address, EmuTime /*time*/) const
{
	if (int channel = waveChannel(address, false); channel >= 0) {
		return readWave(unsigned(channel), address);
	}
	// 0x80..0x9F : freq volume block, write only  (Real, Compatible)
	// 0xA0..0xBF : freq volume block              (Plus)
	// 0xC0..0xFF : deformation register or no function
	return 0xFF;
}

// The byte at an address of a channel's RAM as the CPU sees it: a write that
// is still pending on the stream counts.
int8_t SCC::currentByte(unsigned channel, unsigned address) const
{
	address &= 0x1F;
	for (const auto& e : std::views::reverse(events[channel])) {
		if ((e.kind == Event::WRITE) && (e.address == address)) {
			return narrow_cast<int8_t>(e.data);
		}
	}
	if ((channel == 4) && (currentMode != Mode::Plus)) {
		// channel 5's RAM is written through channel 4's
		for (const auto& e : std::views::reverse(events[3])) {
			if ((e.kind == Event::WRITE) && (e.address == address)) {
				return narrow_cast<int8_t>(e.data);
			}
		}
	}
	return wave[channel][address];
}

uint8_t SCC::readWave(unsigned channel, unsigned address) const
{
	if (!rotate[channel]) {
		return currentByte(channel, address);
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
	// during a CPU access in this mode is not settled; this keeps the
	// documented observation: with bit 6 the shared RAM follows channel
	// 5's pointer, with bit 7 channel 4's.
	unsigned ptr = ((channel == 3) && (currentMode != Mode::Plus) &&
	                ((deformValue & 0xC0) == 0x40)) ? 4 : channel;
	return currentByte(channel, pos[ptr]);
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

	if (int channel = waveChannel(address, true); channel >= 0) {
		// Lands on the RAM at its own cycle of the stream, and holds the
		// RAM's address for the bus cycle, see runSample().
		queueEvent(unsigned(channel), {accessCycle(time), Event::WRITE, uint8_t(address & 0x1F), value});
		return;
	}
	bool freqVol = false;
	bool deform  = false;
	switch (currentMode) {
	case Mode::Real:
		// 0x80..0x9F : freq volume block
		// 0xA0..0xDF : no function
		// 0xE0..0xFF : deformation register
		freqVol = (0x80 <= address) && (address < 0xA0);
		deform  = (address >= 0xE0);
		break;
	case Mode::Compatible:
		// 0x80..0x9F : freq volume block
		// 0xA0..0xBF : ignore write wave form 5
		// 0xC0..0xDF : deformation register
		// 0xE0..0xFF : no function
		freqVol = (0x80 <= address) && (address < 0xA0);
		deform  = (0xC0 <= address) && (address < 0xE0);
		break;
	case Mode::Plus:
		// 0xA0..0xBF : freq volume block
		// 0xC0..0xDF : deformation register
		// 0xE0..0xFF : no function
		freqVol = (0xA0 <= address) && (address < 0xC0);
		deform  = (0xC0 <= address) && (address < 0xE0);
		break;
	default:
		UNREACHABLE;
	}
	if (freqVol) {
		if (setFreqVol(address, value)) {
			// a frequency write reloads the counter, at its own cycle
			unsigned channel = (address & 0x0F) / 2;
			queueEvent(channel, {accessCycle(time), Event::RELOAD, 0,
			                     uint8_t((deformValue & 0x20) ? 1 : 0)});
		}
	} else if (deform) {
		setDeformReg(value);
	}
}

float SCC::getAmplificationFactorImpl() const
{
	return 1.0f / 128.0f;
}

// The product of the chip's serial multiplier: (wave * volume) >> 4,
// verified bit-exact against a model of the multiplier for all 4096 inputs.
// The result is an integer value, but we store it as a float because that
// is what the output buffers hold.
static constexpr float adjust(int8_t wav, uint8_t vol)
{
	return float((int(wav) * vol) >> 4);
}

void SCC::writeWave(unsigned channel, unsigned address, uint8_t value)
{
	// in Real mode channels 4 and 5 are one RAM, addressed as channel 4
	assert(channel < 5);
	assert((channel != 4) || (currentMode != Mode::Real));

	if (!readOnly[channel]) {
		unsigned p = address & 0x1F;
		wave[channel][p] = narrow_cast<int8_t>(value);
		if ((currentMode != Mode::Plus) && (channel == 3)) {
			// copy waveform 4 -> waveform 5
			wave[4][p] = wave[3][p];
		}
	}
}

// The period a channel steps at under the current deformation mode: the
// counter counts down from this value, plus one cycle for the reload.
unsigned SCC::effectivePeriod(unsigned channel) const
{
	unsigned org = orgPeriod[channel];
	if (deformValue & 2) return org & 0xFF;
	if (deformValue & 1) return org >> 8;
	return org;
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
// latch. The multiplier reads its bits from the latch, so it sees the byte
// the pointer was at when the latch was last captured, normally one
// position behind, and when a capture lands inside the 8 cycles it reads
// bits in, the low bits come from the old byte and the high bits from the
// new one.
//
// The sequencer's period is exactly one of our samples, so within a sample
// each latch is captured at a fixed edge: channel 4's on the first edge
// after reset, channel 5's 16 later. Where that sits relative to the
// sample grid can't be known here, only that it is fixed.
bool SCC::isLatched(unsigned channel) const
{
	return (currentMode == Mode::Real) && (channel >= 3);
}

// The cycle of the stream at which a CPU access at 'time' lands. The stream
// has been generated up to some sample at or before 'time' (updateStream()
// is called first); the access is that many cycles into what follows.
uint64_t SCC::accessCycle(EmuTime time)
{
	auto [n, frac] = getEmuClock().getTicksTillAsIntFloat(time);
	return streamCycle + 32 * uint64_t(n) + unsigned(32.0f * frac) + 1;
}

void SCC::queueEvent(unsigned channel, const Event& event)
{
	auto& ev = events[channel];
	// Accesses arrive in order of time and the stream is never more than
	// a few samples behind, so this stays short. Should it not, apply the
	// oldest right away rather than lose it.
	if (ev.size() >= 64) {
		applyEvent(channel, ev.front());
		ev.erase(ev.begin());
	}
	ev.push_back(event);
}

// Apply an event without regard for its cycle.
void SCC::applyEvent(unsigned channel, const Event& event)
{
	switch (event.kind) {
	case Event::READ:
		break;
	case Event::WRITE:
		writeWave(channel, event.address, event.data);
		break;
	case Event::RELOAD:
		reload(channel, event.data & 1);
		break;
	default:
		UNREACHABLE;
	}
}

// A frequency write: the counter is loaded with the (new) frequency, with
// deformation bit 5 the waveform restarts from the top, and the multiplier
// starts over on the byte at the pointer.
void SCC::reload(unsigned channel, bool restart)
{
	counter[channel] = orgPeriod[channel];
	if (restart) pos[channel] = 0;
	multiplier[channel] = {.active = true, .bit = 0, .byte = 0, .vol = volume[channel]};
}

// One sample of a channel for the samples in which something happens,
// exact to the cycle: the frequency counter, the serial multiplier reading
// its 8 bits and writing the output latch, the channel 4/5 capture, and
// whatever the CPU does to the RAM port meanwhile. 'base' is the stream
// cycle before the sample's first edge. Returns the mean output over the
// sample, and leaves the output latch in out[].
float SCC::runSample(unsigned channel, uint64_t base)
{
	auto& mul = multiplier[channel];
	auto& ev = events[channel];
	bool latched = isLatched(channel);
	unsigned capture = latched ? CAPTURE_EDGE[channel - 3] : 33; // 33: none
	float o = out[channel];
	float acc = 0.0f;
	unsigned c = 1;
	while (c <= 32) {
		uint64_t edge = base + c;

		// Two shortcuts, both exact: as long as the CPU is not on the
		// RAM port, a running multiplier can take its remaining bits in
		// one go if nothing else happens until it writes its product,
		// and an idle channel can skip ahead to its next step.
		unsigned busy = 33; // first edge from 'c' on with the CPU on the port
		if (!ev.empty()) {
			busy = unsigned(std::max(ev.front().cycle, edge) - base);
		}
		if (stealUntil[channel] > edge) busy = c;
		unsigned stepEdge = c + remaining(channel);
		if (mul.active) {
			unsigned n = 9 - mul.bit; // edges up to and including the write
			unsigned last = c + n - 1;
			if ((last <= 32) && (last < busy) && (stepEdge >= last) &&
			    ((capture < c) || (capture > last))) {
				uint8_t src = latched ? uint8_t(waveLatch[channel - 3])
				                      : uint8_t(wave[channel][pos[channel]]);
				mul.byte |= uint8_t(src & ~((1u << mul.bit) - 1));
				acc += o * float(n);
				o = adjust(narrow_cast<int8_t>(mul.byte), mul.vol);
				mul.active = false;
				if (advanceCounter(channel, n) != 0) {
					// a step on the write edge itself
					pos[channel] = (pos[channel] + 1) & 31;
					mul = {.active = true, .bit = 0, .byte = 0, .vol = volume[channel]};
				}
				c = last + 1;
				continue;
			}
		} else {
			unsigned until = std::min({stepEdge, busy, 33u});
			if (until > c) {
				unsigned k = until - c; // quiet edges c .. until-1
				acc += o * float(k);
				if ((capture >= c) && (capture < until)) {
					waveLatch[channel - 3] = wave[channel][pos[channel]];
				}
				(void)advanceCounter(channel, k);
				c = until;
				continue;
			}
		}

		// One edge, the general way. In this order: the CPU's access
		// (the RAM's address becomes the CPU's for ACCESS_CYCLES cycles,
		// showing the byte read or written); the multiplier reads a bit
		// from the RAM port, or the latch for channels 4 and 5, or
		// writes its product; channels 4 and 5 capture their latch,
		// still at the address before this edge; the counter reloads on
		// a frequency write or steps the pointer on reaching zero, either
		// of which restarts the multiplier. A step every 8 cycles or
		// fewer (period below 8) therefore never lets the multiplier
		// finish: the output freezes while the pointer keeps running.
		acc += o; // the output during this cycle: the latch after the previous edge

		bool doReload = false;
		bool restart  = false;
		while (!ev.empty() && (ev.front().cycle <= edge)) {
			const auto& e = ev.front();
			switch (e.kind) {
			case Event::READ:
				stealByte [channel] = uint8_t(wave[channel][e.address]);
				stealUntil[channel] = e.cycle + ACCESS_CYCLES;
				break;
			case Event::WRITE:
				writeWave(channel, e.address, e.data);
				stealByte [channel] = e.data;
				stealUntil[channel] = e.cycle + ACCESS_CYCLES;
				break;
			case Event::RELOAD:
				doReload = true;
				restart  = e.data & 1;
				break;
			default:
				UNREACHABLE;
			}
			ev.erase(ev.begin());
		}
		bool stolen = edge < stealUntil[channel];

		if (mul.active) {
			if (mul.bit < 8) {
				uint8_t src = stolen  ? stealByte[channel]
				            : latched ? uint8_t(waveLatch[channel - 3])
				                      : uint8_t(wave[channel][pos[channel]]);
				mul.byte |= uint8_t(((src >> mul.bit) & 1) << mul.bit);
				++mul.bit;
			} else {
				o = adjust(narrow_cast<int8_t>(mul.byte), mul.vol);
				mul.active = false;
			}
		}
		if (c == capture) {
			waveLatch[channel - 3] = stolen ? narrow_cast<int8_t>(stealByte[channel])
			                                : wave[channel][pos[channel]];
		}
		if (doReload) {
			reload(channel, restart);
		} else if (advanceCounter(channel, 1) != 0) {
			pos[channel] = (pos[channel] + 1) & 31;
			mul = {.active = true, .bit = 0, .byte = 0, .vol = volume[channel]};
		}
		++c;
	}
	out[channel] = o;
	return acc * (1.0f / 32.0f);
}

// One sample of a channel: the mean output over it. Takes a shortcut when
// nothing can change within the sample.
float SCC::sample(unsigned channel, uint64_t base)
{
	const auto& mul = multiplier[channel];
	if (events[channel].empty() && (stealUntil[channel] <= base + 1)) {
		if (!mul.active && (remaining(channel) >= 32)) {
			// no step: the counter counts, the latch captures the
			// same byte again
			(void)advanceCounter(channel, 32);
			if (isLatched(channel)) {
				waveLatch[channel - 3] = wave[channel][pos[channel]];
			}
			return out[channel];
		}
		if ((effectivePeriod(channel) < 8) && !(mul.active && (mul.bit == 8))) {
			// the multiplier is restarted before it can finish: the
			// output stays, the pointer runs
			pos[channel] = (pos[channel] + advanceCounter(channel, 32)) & 31;
			if (isLatched(channel)) {
				waveLatch[channel - 3] = wave[channel][pos[channel]];
			}
			multiplier[channel] = {.active = true, .bit = 0, .byte = 0, .vol = volume[channel]};
			return out[channel];
		}
	}
	return runSample(channel, base);
}

// A block of samples of a channel that can't be heard: apply what the CPU
// did, run the counter, keep the latch at the pointer. The output latch is
// quiet, and stays so until a multiplier finishes after the channel is
// audible again.
void SCC::skipBlock(unsigned channel, unsigned num)
{
	for (const auto& e : events[channel]) {
		applyEvent(channel, e);
	}
	events[channel].clear();
	stealUntil[channel] = 0;
	pos[channel] = (pos[channel] + advanceCounter(channel, 32 * num)) & 31;
	if (isLatched(channel)) {
		waveLatch[channel - 3] = wave[channel][pos[channel]];
	}
	multiplier[channel] = Multiplier{};
	out[channel] = 0.0f;
}

// Write a register of the frequency/volume block. Returns whether it was a
// frequency register; the caller decides when the counter reloads.
bool SCC::setFreqVol(unsigned address, uint8_t value)
{
	address &= 0x0F; // region is visible twice
	if (address < 0x0A) {
		// change frequency
		unsigned channel = address / 2;
		orgPeriod[channel] =
			  (address & 1)
			? ((value & 0xF) << 8) | (orgPeriod[channel] & 0xFF)
			: (orgPeriod[channel] & 0xF00) | (value & 0xFF);
		return true;
	} else if (address < 0x0F) {
		// change volume. Takes effect when a multiplier next starts.
		volume[address - 0x0A] = value & 0xF;
	} else {
		// change enable-bits
		ch_enable = value;
	}
	return false;
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
	// Bits 0 and 1 (4 bit / 8 bit frequency) are gates in the frequency
	// counter chain and are read from deformValue on every step, so they
	// take effect right away.
	deformValue = value;
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

// Each of our samples covers 32 master clock cycles, in which the chip's
// output can step several times, at any cycle. A sample carries the mean
// of the output over its 32 cycles: a step counts for the cycles it was in
// effect, wherever in the sample it falls, and no step is lost when they
// come closer than 32 cycles.
void SCC::generateChannels(std::span<float*> bufs, unsigned num)
{
	unsigned enable = ch_enable;
	for (unsigned i = 0; i < 5; ++i, enable >>= 1) {
		if (!(enable & 1) || (!volume[i] && (out[i] == 0.0f))) {
			bufs[i] = nullptr; // channel muted
			skipBlock(i, num);
		} else {
			uint64_t base = streamCycle;
			for (auto j : xrange(num)) {
				bufs[i][j] += sample(i, base);
				base += 32;
			}
		}
	}
	streamCycle += 32 * uint64_t(num);
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
		if (scc.setFreqVol(address, value)) {
			scc.reload((address & 0x0F) / 2, scc.deformValue & 0x20);
		}
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
		// recalculate rotate[5] and readOnly[5]
		setDeformRegHelper(deformValue);

		// the stream starts clean
		streamCycle = 0;
		for (auto& m : multiplier) m = Multiplier{};
		for (auto& ev : events) ev.clear();
		std::ranges::fill(stealUntil, 0);
	}

	if (ar.versionAtLeast(version, 2)) {
		ar.serialize("counter", counter);
	} else {
		std::array<unsigned, 5> count; // cycles since the last reload
		ar.serialize("count", count);
		for (auto channel : xrange(5)) {
			unsigned org = orgPeriod[channel];
			unsigned per = effectivePeriod(channel);
			unsigned n = std::min(count[channel], per);
			unsigned rem = per - n; // what is left to count
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
