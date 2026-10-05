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

#include "ymfm_adpcm.h"

namespace ymfm
{

//*********************************************************
// ADPCM "A" REGISTERS
//*********************************************************

//-------------------------------------------------
//  reset - reset the register state
//-------------------------------------------------

void adpcm_a_registers::reset()
{
	std::fill_n(&m_regdata[0], REGISTERS, 0);

	// initialize the pans to on by default, and max instrument volume;
	// some neogeo homebrews (for example ffeast) rely on this
	m_regdata[0x08] = m_regdata[0x09] = m_regdata[0x0a] =
	m_regdata[0x0b] = m_regdata[0x0c] = m_regdata[0x0d] = 0xdf;
}


//*********************************************************
// ADPCM "A" CHANNEL
//*********************************************************

//-------------------------------------------------
//  adpcm_a_channel - constructor
//-------------------------------------------------

adpcm_a_channel::adpcm_a_channel(ymfm_interface &intf, adpcm_a_registers &regs, uint32_t choffs) :
	m_choffs(uint8_t(choffs)),
	m_playing(false),
	m_curnibble(0),
	m_curbyte(0),
	m_curaddress(0),
	m_accumulator(0),
	m_step_index(0),
	m_regs(regs),
	m_intf(intf)
{
}


//-------------------------------------------------
//  reset - reset the channel state
//-------------------------------------------------

void adpcm_a_channel::reset()
{
	m_playing = false;
	m_curnibble = 0;
	m_curbyte = 0;
	m_curaddress = 0;
	m_accumulator = 0;
	m_step_index = 0;
}


//-------------------------------------------------
//  keyonoff - signal key on/off
//-------------------------------------------------

void adpcm_a_channel::keyonoff(bool on)
{
	// QUESTION: repeated key ons restart the sample?
	m_playing = on;
	if (m_playing)
	{
		m_curaddress = m_regs.ch_start(m_choffs);
		m_curnibble = 0;
		m_curbyte = 0;
		m_accumulator = 0;
		m_step_index = 0;
	}
}


//-------------------------------------------------
//  clock - master clocking function
//-------------------------------------------------

void adpcm_a_channel::clock()
{
	// if not playing, just output 0
	if (!m_playing)
	{
		m_accumulator = 0;
		return;
	}

	// if we're about to read nibble 0, fetch the data
	uint8_t data;
	if (m_curnibble == 0)
	{
		// stop when we hit the end address; apparently only low 20 bits are used for
		// comparison on the YM2610: this affects sample playback in some games, for
		// example twinspri character select screen music will skip some samples if
		// this is not correct
		//
		// note also: end address is inclusive, so wait until we are about to fetch
		// the sample just after the end before stopping; this is needed for nitd's
		// jump sound, for example
		uint32_t end = m_regs.ch_end(m_choffs) + 1;
		if (((m_curaddress ^ end) & 0xfffff) == 0)
		{
			m_playing = false;
			m_accumulator = 0;
			return;
		}

		m_curbyte = m_intf.ymfm_external_read(ACCESS_ADPCM_A, m_curaddress++);
		data = m_curbyte >> 4;
		m_curnibble = 1;
	}

	// otherwise just extract from the previosuly-fetched byte
	else
	{
		data = m_curbyte & 0xf;
		m_curnibble = 0;
	}

	// compute the ADPCM delta
	static uint16_t const s_steps[49] =
	{
		 16,  17,   19,   21,   23,   25,   28,
		 31,  34,   37,   41,   45,   50,   55,
		 60,  66,   73,   80,   88,   97,  107,
		118, 130,  143,  157,  173,  190,  209,
		230, 253,  279,  307,  337,  371,  408,
		449, 494,  544,  598,  658,  724,  796,
		876, 963, 1060, 1166, 1282, 1411, 1552
	};
	int32_t delta = (2 * bitfield(data, 0, 3) + 1) * s_steps[m_step_index] / 8;
	if (bitfield(data, 3))
		delta = -delta;

	// the 12-bit accumulator wraps on the ym2610 and ym2608 (like the msm5205)
	m_accumulator = int16_t((m_accumulator + delta) & 0xfff);

	// adjust ADPCM step
	static int8_t const s_step_inc[8] = { -1, -1, -1, -1, 2, 5, 7, 9 };
	m_step_index = int8_t(std::clamp(m_step_index + s_step_inc[bitfield(data, 0, 3)], 0, 48));
}


//-------------------------------------------------
//  silent - output stays zero until a register write
//-------------------------------------------------

bool adpcm_a_channel::silent() const
{
	// A stopped channel forces the accumulator to 0 on the next clock that
	// includes it. Until that clock, sample() still emits the held value.
	// Key-on only happens from a register write.
	if (!m_playing && m_accumulator == 0)
		return true;

	// Instrument level, total level and pan are registers. clock() does
	// not change them, and a write ends the current buffer first.
	return make_output_plan().pan_mask == 0;
}


//-------------------------------------------------
//  make_output_plan - read the register fields
//  that hold for the whole buffer
//-------------------------------------------------

adpcm_a_channel::output_plan adpcm_a_channel::make_output_plan() const
{
	output_plan plan;

	// volume combines instrument and total levels
	int vol = (m_regs.ch_instrument_level(m_choffs) ^ 0x1f) + (m_regs.total_level() ^ 0x3f);

	// convert into a shift and a multiplier
	// QUESTION: verify this from other sources
	plan.mul = 15 - (vol & 7);
	plan.shift = 4 + 1 + (vol >> 3);

	// a maximum combined volume adds nothing, and neither does a closed pan
	plan.pan_mask = 0;
	if (vol < 63)
	{
		if (m_regs.ch_pan_left(m_choffs))
			plan.pan_mask |= 1;
		if (m_regs.ch_pan_right(m_choffs))
			plan.pan_mask |= 2;
	}
	return plan;
}



//*********************************************************
// ADPCM "A" ENGINE
//*********************************************************

//-------------------------------------------------
//  adpcm_a_engine - constructor
//-------------------------------------------------

adpcm_a_engine::adpcm_a_engine(ymfm_interface &intf) :
	m_channel(generate_array<CHANNELS>([&](size_t chnum) {
		return adpcm_a_channel(intf, m_regs, uint32_t(chnum)); }))
{
}


//-------------------------------------------------
//  reset - reset the engine state
//-------------------------------------------------

void adpcm_a_engine::reset()
{
	// reset register state
	m_regs.reset();

	// reset each channel
	for (auto &chan : m_channel)
		chan.reset();
}


//-------------------------------------------------
//  generate - one channel for the whole buffer, then the next
//-------------------------------------------------

void adpcm_a_engine::generate(std::span<float*, CHANNELS> buffers, unsigned num, uint32_t envStart)
{
	for (int chnum = 0; chnum < CHANNELS; ++chnum) {
		auto& channel = m_channel[chnum];
		if (channel.resting())
			continue;
		// Volume and pan are registers, so they are read once per buffer.
		// An empty pan mask means this channel adds nothing.
		const auto plan = channel.make_output_plan();
		float* buf = (plan.pan_mask != 0) ? buffers[chnum] : nullptr;
		uint32_t env = envStart;
		// Channels 0-3 clock on every ADPCM tick. Channels 4-5 clock on
		// every other tick, when envelope bit 2 is clear.
		const bool low = chnum < 4;
		if (buf == nullptr) {
			for (unsigned i = 0; i < num; ++i) {
				env = step_eg_counter(env);
				if ((env & 3) == 0 && (low || (env & 4) == 0))
					channel.clock();
			}
		} else {
			// Only clock() changes the accumulator, so the scaled sample is
			// recomputed there instead of once per sample. A closed pan side
			// keeps adding zero.
			const bool pan_left = (plan.pan_mask & 1) != 0;
			const bool pan_right = (plan.pan_mask & 2) != 0;
			float left = 0.0f;
			float right = 0.0f;
			auto rescale = [&] {
				float value = float(channel.sample(plan));
				left = pan_left ? value : 0.0f;
				right = pan_right ? value : 0.0f;
			};
			rescale();
			for (unsigned i = 0; i < num; ++i) {
				env = step_eg_counter(env);
				if ((env & 3) == 0 && (low || (env & 4) == 0)) {
					channel.clock();
					rescale();
				}
				unsigned pos = i * 2;
				buf[pos + 0] += left;
				buf[pos + 1] += right;
			}
		}
	}
}


//-------------------------------------------------
//  write - handle writes to the ADPCM-A registers
//-------------------------------------------------

void adpcm_a_engine::write(uint32_t regnum, uint8_t data)
{
	// store the raw value to the register array;
	// most writes are passive, consumed only when needed
	m_regs.write(regnum, data);

	// actively handle writes to the control register
	if (regnum == 0x00)
		for (int chnum = 0; chnum < CHANNELS; chnum++)
			if (bitfield(data, chnum))
				m_channel[chnum].keyonoff(bitfield(~data, 7));
}



//*********************************************************
// ADPCM "B" REGISTERS
//*********************************************************

//-------------------------------------------------
//  reset - reset the register state
//-------------------------------------------------

void adpcm_b_registers::reset()
{
	std::fill_n(&m_regdata[0], REGISTERS, 0);

	// default limit to wide open
	m_regdata[0x0c] = m_regdata[0x0d] = 0xff;
}



//*********************************************************
// ADPCM "B" CHANNEL
//*********************************************************

//-------------------------------------------------
//  adpcm_b_channel - constructor
//-------------------------------------------------

adpcm_b_channel::adpcm_b_channel(ymfm_interface &intf, adpcm_b_registers &regs) :
	m_status(STATUS_BRDY),
	m_curnibble(0),
	m_curbyte(0),
	m_dummy_read(0),
	m_position(0),
	m_curaddress(0),
	m_accumulator(0),
	m_prev_accum(0),
	m_adpcm_step(STEP_MIN),
	m_cpu_write_active(false),
	m_regs(regs),
	m_intf(intf)
{
}


//-------------------------------------------------
//  reset - reset the channel state
//-------------------------------------------------

void adpcm_b_channel::reset()
{
	m_status = STATUS_BRDY;
	m_curnibble = 0;
	m_curbyte = 0;
	m_dummy_read = 0;
	m_position = 0;
	m_curaddress = 0;
	m_accumulator = 0;
	m_prev_accum = 0;
	m_adpcm_step = STEP_MIN;
	m_cpu_write_active = false;
}


//-------------------------------------------------
//  consume_nibble - one sample after the position has wrapped
//-------------------------------------------------

bool adpcm_b_channel::consume_nibble()
{
	// if we're about to process nibble 0, fetch sample
	if (m_curnibble == 0)
	{
		// playing from RAM/ROM
		if (m_regs.external())
			m_curbyte = m_intf.ymfm_external_read(ACCESS_ADPCM_B, m_curaddress);
	}

	// extract the nibble from our current byte
	uint8_t data = uint8_t(m_curbyte << (4 * m_curnibble)) >> 4;
	m_curnibble ^= 1;

	// we just processed the last nibble
	if (m_curnibble == 0)
	{
		// if playing from RAM/ROM, check the end/limit address or advance
		if (m_regs.external())
		{
			// handle the sample end, either repeating or stopping
			if (at_end())
			{
				// if repeating, go back to the start
				if (m_regs.repeat())
					load_start();

				// otherwise, done; set the EOS bit
				else
				{
					m_accumulator = 0;
					m_prev_accum = 0;
					m_status = (m_status & ~STATUS_PLAYING) | STATUS_EOS;
					return false;
				}
			}

			// wrap at the limit address
			else if (at_limit())
				m_curaddress = 0;

			// otherwise, advance the current address
			else
			{
				m_curaddress++;
				m_curaddress &= 0xffffff;
			}
		}

		// if CPU-driven, copy the next byte and request more
		else
		{
			m_curbyte = m_regs.cpudata();
			m_status |= STATUS_BRDY;
		}
	}

	// remember previous value for interpolation
	m_prev_accum = m_accumulator;

	// forecast to next forecast: 1/8, 3/8, 5/8, 7/8, 9/8, 11/8, 13/8, 15/8
	int32_t delta = (2 * bitfield(data, 0, 3) + 1) * m_adpcm_step / 8;
	if (bitfield(data, 3))
		delta = -delta;

	// add and clamp to 16 bits
	m_accumulator = int16_t(std::clamp(int(m_accumulator) + delta, -32768, 32767));

	// scale the ADPCM step: 0.9, 0.9, 0.9, 0.9, 1.2, 1.6, 2.0, 2.4
	static uint8_t const s_step_scale[8] = { 57, 57, 57, 57, 77, 102, 128, 153 };
	m_adpcm_step = int16_t(std::clamp(int(m_adpcm_step) * s_step_scale[bitfield(data, 0, 3)] / 64,
	                                  int(STEP_MIN), int(STEP_MAX)));
	return true;
}


//-------------------------------------------------
//  clock_n - several clocks, batching position steps
//-------------------------------------------------

void adpcm_b_channel::clock_n(unsigned num)
{
	if (num == 0)
		return;

	// Not decoding: a clock only clears PLAYING. One store covers the run.
	if (!decoding()) {
		m_status &= ~STATUS_PLAYING;
		return;
	}

	const uint32_t delta = m_regs.delta_n();
	// Adding zero never reaches the next nibble.
	if (delta == 0)
		return;

	while (num != 0) {
		// Clocks until and including the next 16-bit overflow.
		uint32_t room = 0x10000u - m_position;
		uint32_t steps = (room + delta - 1) / delta;
		if (steps > num) {
			m_position = uint16_t(uint32_t(m_position) + uint64_t(num) * delta);
			return;
		}
		// The low 16 bits are the position after the overflowing add.
		m_position = uint16_t(uint32_t(m_position) + uint64_t(steps) * delta);
		num -= steps;
		// End-without-repeat stops here. Later clocks would only clear
		// PLAYING, which consume_nibble() already cleared.
		if (!consume_nibble())
			return;
	}
}


//-------------------------------------------------
//  generate - num clocks with output
//-------------------------------------------------

void adpcm_b_channel::generate(float* buffer, unsigned num, const output_plan &plan)
{
	if (num == 0)
		return;

	const bool pan_left = (plan.pan_mask & 1) != 0;
	const bool pan_right = (plan.pan_mask & 2) != 0;

	// The decode state and delta-N are registers, so they are read once.
	// Without decoding the position never moves, and neither does it without
	// a step, so in both cases the held sample repeats for the whole run.
	uint32_t delta = 0;
	if (decoding())
		delta = m_regs.delta_n();
	else
		m_status &= ~STATUS_PLAYING;

	auto tick = [&] {
		// End-without-repeat zeroes both interpolator ends, so the rest of
		// the run holds that zero.
		if (delta != 0 && !advance(delta))
			delta = 0;
		return float(sample(plan));
	};

	if (pan_left && pan_right) {
		for (unsigned i = 0; i < num; ++i) {
			float value = tick();
			unsigned pos = i * 2;
			buffer[pos + 0] += value;
			buffer[pos + 1] += value;
		}
	} else if (pan_left) {
		for (unsigned i = 0; i < num; ++i)
			buffer[i * 2] += tick();
	} else {
		for (unsigned i = 0; i < num; ++i)
			buffer[i * 2 + 1] += tick();
	}
}


//-------------------------------------------------
//  silent - output stays zero until a register write
//-------------------------------------------------

bool adpcm_b_channel::silent() const
{
	// Not advancing: a clock returns before it touches the accumulators.
	// Playback itself starts only from a register write. A held sample is
	// silent only when both ends of the interpolator are already zero.
	if (!decoding() && m_accumulator == 0 && m_prev_accum == 0)
		return true;

	// Level and pan are registers. clock() does not change them, and a
	// write ends the current buffer first.
	return pan_mask() == 0;
}


//-------------------------------------------------
//  read - handle special register reads
//-------------------------------------------------

uint8_t adpcm_b_channel::peek(uint32_t regnum) const
{
	// Observe the next CPU read without consuming dummy reads, advancing RAM,
	// changing EOS/BRDY or invoking a potentially destructive host read.
	if (regnum == 0x08 && !m_regs.execute() && !m_regs.record() && m_regs.external())
	{
		if (m_cpu_write_active)
			return m_regs.cpudata();
		if (m_dummy_read == 0)
			return m_intf.ymfm_external_peek(ACCESS_ADPCM_B, m_curaddress);
	}
	return 0;
}

uint8_t adpcm_b_channel::read(uint32_t regnum)
{
	uint8_t result = 0;

	// register 8 reads over the bus under some conditions
	if (regnum == 0x08 && !m_regs.execute() && !m_regs.record() && m_regs.external())
	{
		// A mode change alone does not terminate an unfinished RAM writer.
		// Until RESET, the CPU sees its last data-buffer byte (Makoto V4).
		if (m_cpu_write_active)
			return m_regs.cpudata();

		// two dummy reads are consumed first
		if (m_dummy_read != 0)
		{
			load_start();
			m_dummy_read--;
		}

		// read the data
		else
		{
			// read from outside of the chip
			result = m_intf.ymfm_external_read(ACCESS_ADPCM_B, m_curaddress);

			// did we hit the end? if so, signal EOS
			if (at_end())
			{
				m_status = STATUS_EOS | STATUS_BRDY;
			}
			else
			{
				// signal ready
				m_status = STATUS_BRDY;
			}

			// The limit is inclusive: consume its last byte before wrapping.
			if (at_limit())
				m_curaddress = 0;
			else
				m_curaddress++;
		}
	}
	return result;
}


//-------------------------------------------------
//  write - handle special register writes
//-------------------------------------------------

void adpcm_b_channel::write(uint32_t regnum, uint8_t value)
{
	// register 0 can do a reset; also use writes here to reset the
	// dummy read counter
	if (regnum == 0x00)
	{
		if (m_regs.execute())
		{
			load_start();
		}
		else
			m_status &= ~STATUS_EOS;
		if (m_regs.resetflag())
			reset();
		if (m_regs.external())
			m_dummy_read = 2;
	}

	// register 8 writes over the bus under some conditions
	else if (regnum == 0x08)
	{
		// if writing from the CPU during execute, clear the ready flag
		if (m_regs.execute() && !m_regs.record() && !m_regs.external())
			m_status &= ~STATUS_BRDY;

		// if writing during "record", pass through as data
		else if (!m_regs.execute() && m_regs.record() && m_regs.external())
		{
			// clear out dummy reads and set start address
			if (m_dummy_read != 0)
			{
				load_start();
				m_dummy_read = 0;
			}

			// The end register describes an inclusive chunk. Keep the CPU
			// address one past its last byte once the transfer has stopped.
			uint32_t end = (m_regs.end() + 1) << address_shift();
			if (m_curaddress != end)
			{
				m_intf.ymfm_external_write(ACCESS_ADPCM_B, m_curaddress++, value);
				m_cpu_write_active = true;
			}

			if (m_curaddress == end)
			{
				m_cpu_write_active = false;
				m_status = STATUS_EOS | STATUS_BRDY;
			}
			else
				m_status = STATUS_BRDY;
		}
	}
}


//-------------------------------------------------
//  address_shift - compute the current address
//  shift amount based on register settings
//-------------------------------------------------

uint32_t adpcm_b_channel::address_shift() const
{
	// if ROM or 8-bit DRAM, shift is 5 bits
	if (m_regs.rom_ram())
		return 5;
	if (m_regs.dram_8bit())
		return 5;

	// otherwise, shift is 2 bits
	return 2;
}


//-------------------------------------------------
//  load_start - load the start address and
//  initialize the state
//-------------------------------------------------

void adpcm_b_channel::load_start()
{
	m_status = (m_status & ~STATUS_EOS) | STATUS_PLAYING;
	m_curaddress = m_regs.external() ? (m_regs.start() << address_shift()) : 0;
	m_curnibble = 0;
	m_curbyte = 0;
	m_position = 0;
	m_accumulator = 0;
	m_prev_accum = 0;
	m_adpcm_step = STEP_MIN;
	m_cpu_write_active = false;
}



//*********************************************************
// ADPCM "B" ENGINE
//*********************************************************

//-------------------------------------------------
//  adpcm_b_engine - constructor
//-------------------------------------------------

adpcm_b_engine::adpcm_b_engine(ymfm_interface &intf) :
	m_channel(intf, m_regs)
{
}


//-------------------------------------------------
//  reset - reset the engine state
//-------------------------------------------------

void adpcm_b_engine::reset()
{
	// reset registers
	m_regs.reset();

	// reset each channel
	m_channel.reset();
}


//-------------------------------------------------
//  generate - the whole buffer for the single channel
//-------------------------------------------------

void adpcm_b_engine::generate(float* buffer, unsigned num)
{
	if (m_channel.resting())
		return;
	// Level and pan are registers, so they are read once per buffer. An empty
	// pan mask means this channel adds nothing.
	const auto plan = m_channel.make_output_plan();
	if (buffer == nullptr || plan.pan_mask == 0)
		m_channel.clock_n(num);
	else
		m_channel.generate(buffer, num, plan);
}


//-------------------------------------------------
//  write - handle writes to the ADPCM-B registers
//-------------------------------------------------

void adpcm_b_engine::write(uint32_t regnum, uint8_t data)
{
	// store the raw value to the register array;
	// most writes are passive, consumed only when needed
	m_regs.write(regnum, data);

	// let the channel handle any special writes
	m_channel.write(regnum, data);
}

}
