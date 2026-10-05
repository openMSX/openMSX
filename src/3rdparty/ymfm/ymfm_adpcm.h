// BSD 3-Clause License
//
// Copyright (c) 2021, Aaron Giles
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef YMFM_ADPCM_H
#define YMFM_ADPCM_H

#pragma once

#include "ymfm.h"

#include "stl.hh"

#include <array>
#include <span>

namespace ymfm
{

// ======================> adpcm_a_registers

//
// ADPCM-A register map:
//
//      System-wide registers:
//           00 x------- Dump (disable=1) or keyon (0) control
//              --xxxxxx Mask of channels to dump or keyon
//           01 --xxxxxx Total level
//           02 xxxxxxxx Test register
//        08-0D x------- Pan left
//              -x------ Pan right
//              ---xxxxx Instrument level
//        10-15 xxxxxxxx Start address (low)
//        18-1D xxxxxxxx Start address (high)
//        20-25 xxxxxxxx End address (low)
//        28-2D xxxxxxxx End address (high)
//
class adpcm_a_registers
{
public:
	// constants
	static constexpr uint32_t CHANNELS = 6;
	static constexpr uint32_t REGISTERS = 0x30;

	// constructor
	adpcm_a_registers() { }

	// reset to initial state
	void reset();

	// save/restore
	template<typename Archive>
	void serialize(Archive& ar, unsigned /*version*/)
	{
		ar.serialize("regdata", m_regdata);
	}


	// direct read/write access
	uint8_t read(uint32_t index) const { return m_regdata[index]; }
	void write(uint32_t index, uint8_t data) { m_regdata[index] = data; }

	// system-wide registers
	uint32_t total_level() const                        { return bitfield(m_regdata[0x01], 0, 6); }

	// per-channel registers
	uint32_t ch_pan_left(uint32_t choffs) const         { return bitfield(m_regdata[choffs + 0x08], 7); }
	uint32_t ch_pan_right(uint32_t choffs) const        { return bitfield(m_regdata[choffs + 0x08], 6); }
	uint32_t ch_instrument_level(uint32_t choffs) const { return bitfield(m_regdata[choffs + 0x08], 0, 5); }
	uint32_t ch_start(uint32_t choffs) const            { return m_regdata[choffs + 0x10] | (m_regdata[choffs + 0x18] << 8); }
	uint32_t ch_end(uint32_t choffs) const              { return m_regdata[choffs + 0x20] | (m_regdata[choffs + 0x28] << 8); }

	// per-channel writes
	void write_start(uint32_t choffs, uint32_t address)
	{
		write(choffs + 0x10, address);
		write(choffs + 0x18, address >> 8);
	}
	void write_end(uint32_t choffs, uint32_t address)
	{
		write(choffs + 0x20, address);
		write(choffs + 0x28, address >> 8);
	}

private:
	// internal state
	std::array<uint8_t, REGISTERS> m_regdata;         // register data
};


// ======================> adpcm_a_channel

class adpcm_a_channel
{
public:
	// constructor
	adpcm_a_channel(ymfm_interface &intf, adpcm_a_registers &regs, uint32_t choffs);

	// reset the channel state
	void reset();

	// save/restore
	template<typename Archive>
	void serialize(Archive& ar, unsigned /*version*/)
	{
		ar.serialize("playing",     m_playing,
		             "curnibble",   m_curnibble,
		             "curbyte",     m_curbyte,
		             "curaddress",  m_curaddress,
		             "accumulator", m_accumulator,
		             "step_index",  m_step_index);
	}

	// signal key on/off
	void keyonoff(bool on);

	// master clockingfunction
	void clock();

	// True when every sample this channel can produce is zero until the
	// next register write. clock() still has to run.
	bool silent() const;

	// Stopped with a zero accumulator: clock() would only store that zero.
	bool resting() const { return m_playing == 0 && m_accumulator == 0; }

	// Register fields that output() reads. A register write ends the current
	// buffer, so these hold for every sample of one generate().
	struct output_plan
	{
		int8_t mul;        // volume multiplier
		uint8_t shift;     // volume shift, accumulator downshift included
		uint8_t pan_mask;  // one bit per output this channel feeds
	};

	// Read those fields once, before the sample loop. An empty pan mask means
	// this channel adds nothing.
	output_plan make_output_plan() const;

	// Scaled sample for the current accumulator, which only clock() changes.
	int16_t sample(const output_plan &plan) const
	{
		// m_accumulator is a 12-bit value; shift up to sign-extend;
		// the downshift is incorporated into the plan's shift
		return int16_t(((int16_t(m_accumulator << 4) * plan.mul) >> plan.shift) & ~3);
	}

private:
	// internal state
	uint32_t const m_choffs;              // channel offset
	uint32_t m_playing;                   // currently playing?
	uint32_t m_curnibble;                 // index of the current nibble
	uint32_t m_curbyte;                   // current byte of data
	uint32_t m_curaddress;                // current address
	int32_t m_accumulator;                // accumulator
	int32_t m_step_index;                 // index in the stepping table
	adpcm_a_registers &m_regs;            // reference to registers
	ymfm_interface &m_intf;               // memory reads
};


// ======================> adpcm_a_engine

class adpcm_a_engine
{
public:
	static constexpr int CHANNELS = adpcm_a_registers::CHANNELS;

	// constructor; the channels point into m_regs, so copying is not safe
	adpcm_a_engine(ymfm_interface &intf);
	adpcm_a_engine(const adpcm_a_engine &) = delete;
	adpcm_a_engine &operator=(const adpcm_a_engine &) = delete;

	// reset our status
	void reset();

	// save/restore
	template<typename Archive>
	void serialize(Archive& ar, unsigned /*version*/)
	{
		ar.serialize("regs", m_regs);
		for (int chnum = 0; chnum < CHANNELS; ++chnum) {
			ar.serialize("channel", m_channel[chnum]);
		}
	}



	// Whole buffer, one channel at a time. envStart is the FM envelope counter
	// before this buffer, and this walks the same grid. A nullptr entry skips
	// output. A resting channel is not clocked.
	void generate(std::span<float*, CHANNELS> buffers, unsigned num, uint32_t envStart);

	// True when this channel's output() will add zero until the next register write.
	bool silent(unsigned chnum) const { return m_channel[chnum].silent(); }

	// write to the ADPCM-A registers
	void write(uint32_t regnum, uint8_t data);

	// set the start/end address for a channel (for hardcoded YM2608 percussion)
	void set_start_end(uint8_t chnum, uint16_t start, uint16_t end)
	{
		m_regs.write_start(chnum, start);
		m_regs.write_end(chnum, end);
	}

	// return a reference to our registers
	adpcm_a_registers &regs() { return m_regs; }
	const adpcm_a_registers &regs() const { return m_regs; }

private:
	// internal state
	adpcm_a_registers m_regs;                          // registers
	std::array<adpcm_a_channel, CHANNELS> m_channel;   // the six channels
};


// ======================> adpcm_b_registers

//
// ADPCM-B register map:
//
//      System-wide registers:
//           00 x------- Start of synthesis/analysis
//              -x------ Record
//              --x----- External/manual driving
//              ---x---- Repeat playback
//              ----x--- Speaker off
//              -------x Reset
//           01 x------- Pan left
//              -x------ Pan right
//              ----x--- Start conversion
//              -----x-- DAC enable
//              ------x- DRAM access (1=8-bit granularity; 0=1-bit)
//              -------x RAM/ROM (1=ROM, 0=RAM)
//           02 xxxxxxxx Start address (low)
//           03 xxxxxxxx Start address (high)
//           04 xxxxxxxx End address (low)
//           05 xxxxxxxx End address (high)
//           06 xxxxxxxx Prescale value (low)
//           07 -----xxx Prescale value (high)
//           08 xxxxxxxx CPU data/buffer
//           09 xxxxxxxx Delta-N frequency scale (low)
//           0a xxxxxxxx Delta-N frequency scale (high)
//           0b xxxxxxxx Level control
//           0c xxxxxxxx Limit address (low)
//           0d xxxxxxxx Limit address (high)
//           0e xxxxxxxx DAC data [YM2608/10]
//           0f xxxxxxxx PCM data [YM2608/10]
//           0e xxxxxxxx DAC data high [Y8950]
//           0f xx------ DAC data low [Y8950]
//           10 -----xxx DAC data exponent [Y8950]
//
class adpcm_b_registers
{
public:
	// constants
	static constexpr uint32_t REGISTERS = 0x11;

	// constructor
	adpcm_b_registers() { }

	// reset to initial state
	void reset();

	// save/restore
	template<typename Archive>
	void serialize(Archive& ar, unsigned /*version*/)
	{
		ar.serialize("regdata", m_regdata);
	}

	// direct read/write access
	uint8_t read(uint32_t index) const { return m_regdata[index]; }
	void write(uint32_t index, uint8_t data) { m_regdata[index] = data; }

	// system-wide registers
	uint32_t execute() const          { return bitfield(m_regdata[0x00], 7); }
	uint32_t record() const           { return bitfield(m_regdata[0x00], 6); }
	uint32_t external() const         { return bitfield(m_regdata[0x00], 5); }
	uint32_t repeat() const           { return bitfield(m_regdata[0x00], 4); }
	uint32_t resetflag() const        { return bitfield(m_regdata[0x00], 0); }
	uint32_t pan_left() const         { return bitfield(m_regdata[0x01], 7); }
	uint32_t pan_right() const        { return bitfield(m_regdata[0x01], 6); }
	uint32_t dram_8bit() const        { return bitfield(m_regdata[0x01], 1); }
	uint32_t rom_ram() const          { return bitfield(m_regdata[0x01], 0); }
	uint32_t start() const            { return m_regdata[0x02] | (m_regdata[0x03] << 8); }
	uint32_t end() const              { return m_regdata[0x04] | (m_regdata[0x05] << 8); }
	uint32_t cpudata() const          { return m_regdata[0x08]; }
	uint32_t delta_n() const          { return m_regdata[0x09] | (m_regdata[0x0a] << 8); }
	uint32_t level() const            { return m_regdata[0x0b]; }
	uint32_t limit() const            { return m_regdata[0x0c] | (m_regdata[0x0d] << 8); }

private:
	// internal state
	std::array<uint8_t, REGISTERS> m_regdata;         // register data
};


// ======================> adpcm_b_channel

class adpcm_b_channel
{
	static constexpr int32_t STEP_MIN = 127;
	static constexpr int32_t STEP_MAX = 24576;

public:
	static constexpr uint8_t STATUS_EOS = 0x01;
	static constexpr uint8_t STATUS_BRDY = 0x02;
	static constexpr uint8_t STATUS_PLAYING = 0x04;

	// constructor
	adpcm_b_channel(ymfm_interface &intf, adpcm_b_registers &regs);

	// reset the channel state
	void reset();

	// save/restore
	template<typename Archive>
	void serialize(Archive& ar, unsigned /*version*/)
	{
		ar.serialize("status",           m_status,
		             "curnibble",        m_curnibble,
		             "curbyte",          m_curbyte,
		             "dummy_read",       m_dummy_read,
		             "position",         m_position,
		             "curaddress",       m_curaddress,
		             "accumulator",      m_accumulator,
		             "prev_accum",       m_prev_accum,
		             "adpcm_step",       m_adpcm_step,
		             "cpu_write_active", m_cpu_write_active);
	}

	// signal key on/off
	void keyonoff(bool on);

	// num clocks. Position steps that do not cross a nibble are applied in
	// one multiply. Each nibble is consume_nibble(), shared with generate().
	// A channel that is not decoding clears PLAYING once and returns.
	void clock_n(unsigned num);

	// True when every sample this channel can produce is zero until the
	// next register write. clock() still has to run.
	bool silent() const;

	// Not playing, both interpolator ends already zero: clock() does not
	// change the accumulators or the playing flag.
	bool resting() const
	{
		return (m_status & STATUS_PLAYING) == 0 && m_accumulator == 0 && m_prev_accum == 0;
	}

	// Register fields that the scaling reads, plus the caller's shift. A
	// register write ends the current buffer, so these hold for every sample
	// of one generate().
	struct output_plan
	{
		uint32_t level;    // linear volume
		uint8_t shift;     // total downshift, the caller's rshift included
		uint8_t pan_mask;  // one bit per output this channel feeds
	};

	// Read those fields once, before the sample loop. An empty pan mask means
	// this channel adds nothing.
	output_plan make_output_plan(uint32_t rshift) const
	{
		return {m_regs.level(), uint8_t(8 + rshift), pan_mask()};
	}

	// Interpolated and scaled sample for the current position, which every
	// clock advances.
	int32_t sample(const output_plan &plan) const
	{
		// do a linear interpolation between samples
		int32_t result = m_prev_accum + int32_t((int64_t(m_accumulator - m_prev_accum) * int32_t(m_position)) >> 16);

		// apply volume (level) in a linear fashion and reduce
		return (result * int32_t(plan.level)) >> plan.shift;
	}

	// num clocks with output, interleaved stereo. Reads the decode state and
	// the position step once, like clock_n() does.
	void generate(float* buffer, unsigned num, const output_plan &plan);

	// return the status register
	uint8_t status() const { return m_status; }

	// handle special register reads
	uint8_t read(uint32_t regnum);
	uint8_t peek(uint32_t regnum) const;

	// handle special register writes
	void write(uint32_t regnum, uint8_t value);

private:
	// Register state that lets a clock advance the position.
	bool decoding() const
	{
		return m_regs.execute() && !m_regs.record() && (m_status & STATUS_PLAYING) != 0;
	}

	// One clock with the buffer's position step, after decoding() held.
	// False when playback stopped at the end address.
	bool advance(uint32_t delta)
	{
		uint32_t position = m_position + delta;
		m_position = uint16_t(position);
		if (position < 0x10000)
			return true;
		return consume_nibble();
	}

	// One bit per output this channel feeds, empty when it adds nothing.
	uint8_t pan_mask() const
	{
		if (m_regs.level() == 0)
			return 0;
		uint8_t mask = 0;
		if (m_regs.pan_left())
			mask |= 1;
		if (m_regs.pan_right())
			mask |= 2;
		return mask;
	}

	// helper - return the current address shift
	uint32_t address_shift() const;

	// One nibble, after the fractional position has already wrapped.
	// Returns false when playback stops at the end address.
	bool consume_nibble();

	// load the start address
	void load_start();

	// limit checker; stops at the last byte of the chunk described by address_shift()
	bool at_limit() const { return (m_curaddress == (((m_regs.limit() + 1) << address_shift()) - 1)); }

	// end checker; stops at the last byte of the chunk described by address_shift()
	bool at_end() const { return (m_curaddress == (((m_regs.end() + 1) << address_shift()) - 1)); }

	// internal state
	uint32_t m_status;              // currently playing?
	uint32_t m_curnibble;           // index of the current nibble
	uint32_t m_curbyte;             // current byte of data
	uint32_t m_dummy_read;          // dummy read tracker
	uint32_t m_position;            // current fractional position
	uint32_t m_curaddress;          // current address
	int32_t m_accumulator;          // accumulator
	int32_t m_prev_accum;           // previous accumulator (for linear interp)
	int32_t m_adpcm_step;           // next forecast
	bool m_cpu_write_active;       // unfinished CPU RAM write sequence
	adpcm_b_registers &m_regs;      // reference to registers
	ymfm_interface &m_intf;         // memory reads and writes
};


// ======================> adpcm_b_engine

class adpcm_b_engine
{
public:
	// constructor; the channel points into m_regs, so copying is not safe
	adpcm_b_engine(ymfm_interface &intf);
	adpcm_b_engine(const adpcm_b_engine &) = delete;
	adpcm_b_engine &operator=(const adpcm_b_engine &) = delete;

	// reset our status
	void reset();

	// save/restore
	template<typename Archive>
	void serialize(Archive& ar, unsigned /*version*/)
	{
		ar.serialize("regs",    m_regs,
		             "channel", m_channel);
	}

	// Whole buffer for the single channel. A nullptr buffer skips output.
	// A resting channel returns without clocking.
	void generate(float* buffer, unsigned num, uint32_t rshift);

	// True when output() will add zero until the next register write.
	bool silent() const { return m_channel.silent(); }

	// read from the ADPCM-B registers
	uint32_t read(uint32_t regnum) { return m_channel.read(regnum); }
	uint8_t peek(uint32_t regnum) const { return m_channel.peek(regnum); }

	// write to the ADPCM-B registers
	void write(uint32_t regnum, uint8_t data);

	// status
	uint8_t status() const { return m_channel.status(); }

	// return a reference to our registers
	adpcm_b_registers &regs() { return m_regs; }
	const adpcm_b_registers &regs() const { return m_regs; }

private:
	// internal state
	adpcm_b_registers m_regs;    // registers
	adpcm_b_channel m_channel;   // the one channel
};

}

#endif // YMFM_ADPCM_H
