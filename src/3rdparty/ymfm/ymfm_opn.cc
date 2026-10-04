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

#include "ymfm_opn.h"
#include "ymfm_fm.ipp"

namespace ymfm
{

//*********************************************************
//  OPN/OPNA REGISTERS
//*********************************************************

//-------------------------------------------------
//  opn_registers_base - constructor
//-------------------------------------------------

template<bool IsOpnA>
opn_registers_base<IsOpnA>::opn_registers_base() :
	m_lfo_counter(0),
	m_lfo_am(0)
{
	// create the waveforms
	for (uint32_t index = 0; index < WAVEFORM_LENGTH; index++)
		m_waveform[0][index] = abs_sin_attenuation(index) | (bitfield(index, 9) << 15);
}


//-------------------------------------------------
//  reset - reset to initial state
//-------------------------------------------------

template<bool IsOpnA>
void opn_registers_base<IsOpnA>::reset()
{
	std::fill_n(&m_regdata[0], REGISTERS, 0);
	if (IsOpnA)
	{
		// enable output on both channels by default
		m_regdata[0xb4] = m_regdata[0xb5] = m_regdata[0xb6] = 0xc0;
		m_regdata[0x1b4] = m_regdata[0x1b5] = m_regdata[0x1b6] = 0xc0;
	}
}


//-------------------------------------------------
//  save_restore - save or restore the data
//-------------------------------------------------

template<bool IsOpnA>
void opn_registers_base<IsOpnA>::save_restore(ymfm_saved_state &state)
{
	if (IsOpnA)
	{
		state.save_restore(m_lfo_counter);
		state.save_restore(m_lfo_am);
	}
	state.save_restore(m_regdata);
}


//-------------------------------------------------
//  operator_map - return an array of operator
//  indices for each channel; for OPN this is fixed
//-------------------------------------------------

template<>
void opn_registers_base<false>::operator_map(operator_mapping &dest) const
{
	// Note that the channel index order is 0,2,1,3, so we bitswap the index.
	//
	// This is because the order in the map is:
	//    carrier 1, carrier 2, modulator 1, modulator 2
	//
	// But when wiring up the connections, the more natural order is:
	//    carrier 1, modulator 1, carrier 2, modulator 2
	static const operator_mapping s_fixed_map =
	{ {
		operator_list(  0,  6,  3,  9 ),  // Channel 0 operators
		operator_list(  1,  7,  4, 10 ),  // Channel 1 operators
		operator_list(  2,  8,  5, 11 ),  // Channel 2 operators
	} };
	dest = s_fixed_map;
}

template<>
void opn_registers_base<true>::operator_map(operator_mapping &dest) const
{
	// Note that the channel index order is 0,2,1,3, so we bitswap the index.
	//
	// This is because the order in the map is:
	//    carrier 1, carrier 2, modulator 1, modulator 2
	//
	// But when wiring up the connections, the more natural order is:
	//    carrier 1, modulator 1, carrier 2, modulator 2
	static const operator_mapping s_fixed_map =
	{ {
		operator_list(  0,  6,  3,  9 ),  // Channel 0 operators
		operator_list(  1,  7,  4, 10 ),  // Channel 1 operators
		operator_list(  2,  8,  5, 11 ),  // Channel 2 operators
		operator_list( 12, 18, 15, 21 ),  // Channel 3 operators
		operator_list( 13, 19, 16, 22 ),  // Channel 4 operators
		operator_list( 14, 20, 17, 23 ),  // Channel 5 operators
	} };
	dest = s_fixed_map;
}


//-------------------------------------------------
//  write - handle writes to the register array
//-------------------------------------------------

template<bool IsOpnA>
bool opn_registers_base<IsOpnA>::write(uint16_t index, uint8_t data, uint32_t &channel, uint32_t &opmask)
{
	assert(index < REGISTERS);

	// writes in the 0xa0-af/0x1a0-af region are handled as latched pairs
	// borrow unused registers 0xb8-bf as temporary holding locations
	if ((index & 0xf0) == 0xa0)
	{
		if (bitfield(index, 0, 2) == 3)
			return false;

		uint32_t latchindex = 0xb8 | bitfield(index, 3);

		// writes to the upper half just latch (only low 6 bits matter)
		if (bitfield(index, 2))
			m_regdata[latchindex] = data & 0x3f;

		// writes to the lower half also apply said latch
		else
		{
			m_regdata[index] = data;
			m_regdata[index | 4] = m_regdata[latchindex];
		}
		return false;
	}
	else if ((index & 0xf8) == 0xb8)
	{
		// registers 0xb8-0xbf are used internally
		return false;
	}

	// everything else is normal
	m_regdata[index] = data;

	// handle writes to the key on index
	if (index == 0x28)
	{
		channel = bitfield(data, 0, 2);
		if (channel == 3)
			return false;
		if (IsOpnA)
			channel += bitfield(data, 2, 1) * 3;
		opmask = bitfield(data, 4, 4);
		return true;
	}
	return false;
}


//-------------------------------------------------
//  clock_noise_and_lfo - clock the noise and LFO,
//  handling clock division, depth, and waveform
//  computations
//-------------------------------------------------

template<bool IsOpnA>
int32_t opn_registers_base<IsOpnA>::clock_noise_and_lfo()
{
	// OPN has no noise generation

	// if LFO not enabled (not present on OPN), quick exit with 0s
	if (!IsOpnA || !lfo_enable())
	{
		m_lfo_counter = 0;

		// special case: if LFO is disabled on OPNA, it basically just keeps the counter
		// at 0; since position 0 gives an AM value of 0x3f, it is important to reflect
		// that here; for example, MegaDrive Venom plays some notes with LFO globally
		// disabled but enabling LFO on the operators, and it expects this added attenutation
		m_lfo_am = IsOpnA ? 0x3f : 0x00;
		return 0;
	}

	// this table is based on converting the frequencies in the applications
	// manual to clock dividers, based on the assumption of a 7-bit LFO value
	static uint8_t const lfo_max_count[8] = { 109, 78, 72, 68, 63, 45, 9, 6 };
	uint32_t subcount = uint8_t(m_lfo_counter++);

	// when we cross the divider count, add enough to zero it and cause an
	// increment at bit 8; the 7-bit value lives from bits 8-14
	if (subcount >= lfo_max_count[lfo_rate()])
	{
		// note: to match the published values this should be 0x100 - subcount;
		// however, tests on the hardware and nuked bear out an off-by-one
		// error exists that causes the max LFO rate to be faster than published
		m_lfo_counter += 0x101 - subcount;
	}

	// AM value is 7 bits, staring at bit 8; grab the low 6 directly
	m_lfo_am = bitfield(m_lfo_counter, 8, 6);

	// first half of the AM period (bit 6 == 0) is inverted
	if (bitfield(m_lfo_counter, 8+6) == 0)
		m_lfo_am ^= 0x3f;

	// PM value is 5 bits, starting at bit 10; grab the low 3 directly
	int32_t pm = bitfield(m_lfo_counter, 10, 3);

	// PM is reflected based on bit 3
	if (bitfield(m_lfo_counter, 10+3))
		pm ^= 7;

	// PM is negated based on bit 4
	return bitfield(m_lfo_counter, 10+4) ? -pm : pm;
}


//-------------------------------------------------
//  lfo_am_offset - return the AM offset from LFO
//  for the given channel
//-------------------------------------------------

template<bool IsOpnA>
uint32_t opn_registers_base<IsOpnA>::lfo_am_offset(uint32_t choffs) const
{
	// shift value for AM sensitivity is [7, 3, 1, 0],
	// mapping to values of [0, 1.4, 5.9, and 11.8dB]
	uint32_t am_shift = (1 << (ch_lfo_am_sens(choffs) ^ 3)) - 1;

	// QUESTION: max sensitivity should give 11.8dB range, but this value
	// is directly added to an x.8 attenuation value, which will only give
	// 126/256 or ~4.9dB range -- what am I missing? The calculation below
	// matches several other emulators, including the Nuked implemenation.

	// raw LFO AM value on OPN is 0-3F, scale that up by a factor of 2
	// (giving 7 bits) before applying the final shift
	return (m_lfo_am << 1) >> am_shift;
}


//-------------------------------------------------
//  cache_operator_data - fill the operator cache
//  with prefetched data
//-------------------------------------------------

template<bool IsOpnA>
void opn_registers_base<IsOpnA>::cache_operator_data(uint32_t choffs, uint32_t opoffs, opdata_cache &cache)
{
	// set up the easy stuff
	cache.waveform = &m_waveform[0][0];

	// get frequency from the channel
	uint32_t block_freq = cache.block_freq = ch_block_freq(choffs);

	// if multi-frequency mode is enabled and this is channel 2,
	// fetch one of the special frequencies
	if (multi_freq() && choffs == 2)
	{
		if (opoffs == 2)
			block_freq = cache.block_freq = multi_block_freq(1);
		else if (opoffs == 10)
			block_freq = cache.block_freq = multi_block_freq(2);
		else if (opoffs == 6)
			block_freq = cache.block_freq = multi_block_freq(0);
	}

	// compute the keycode: block_freq is:
	//
	//     BBBFFFFFFFFFFF
	//     ^^^^???
	//
	// the 5-bit keycode uses the top 4 bits plus a magic formula
	// for the final bit
	uint32_t keycode = bitfield(block_freq, 10, 4) << 1;

	// lowest bit is determined by a mix of next lower FNUM bits
	// according to this equation from the YM2608 manual:
	//
	//   (F11 & (F10 | F9 | F8)) | (!F11 & F10 & F9 & F8)
	//
	// for speed, we just look it up in a 16-bit constant
	keycode |= bitfield(0xfe80, bitfield(block_freq, 7, 4));

	// detune adjustment
	cache.detune = detune_adjustment(op_detune(opoffs), keycode);

	// multiple value, as an x.1 value (0 means 0.5)
	cache.multiple = op_multiple(opoffs) * 2;
	if (cache.multiple == 0)
		cache.multiple = 1;

	// phase step, or PHASE_STEP_DYNAMIC if PM is active; this depends on
	// block_freq, detune, and multiple, so compute it after we've done those
	if (!IsOpnA || lfo_enable() == 0 || ch_lfo_pm_sens(choffs) == 0)
		cache.phase_step = compute_phase_step(choffs, opoffs, cache, 0);
	else
		cache.phase_step = opdata_cache::PHASE_STEP_DYNAMIC;

	// total level, scaled by 8
	cache.total_level = op_total_level(opoffs) << 3;

	// 4-bit sustain level, but 15 means 31 so effectively 5 bits
	cache.eg_sustain = op_sustain_level(opoffs);
	cache.eg_sustain |= (cache.eg_sustain + 1) & 0x10;
	cache.eg_sustain <<= 5;

	// determine KSR adjustment for enevlope rates
	uint32_t ksrval = keycode >> (op_ksr(opoffs) ^ 3);
	cache.eg_rate[EG_ATTACK] = effective_rate(op_attack_rate(opoffs) * 2, ksrval);
	cache.eg_rate[EG_DECAY] = effective_rate(op_decay_rate(opoffs) * 2, ksrval);
	cache.eg_rate[EG_SUSTAIN] = effective_rate(op_sustain_rate(opoffs) * 2, ksrval);
	cache.eg_rate[EG_RELEASE] = effective_rate(op_release_rate(opoffs) * 4 + 2, ksrval);
}


//-------------------------------------------------
//  compute_phase_step - compute the phase step
//-------------------------------------------------

template<bool IsOpnA>
uint32_t opn_registers_base<IsOpnA>::compute_phase_step(uint32_t choffs, uint32_t /*opoffs*/, opdata_cache const &cache, int32_t lfo_raw_pm)
{
	// OPN phase calculation has only a single detune parameter
	// and uses FNUMs instead of keycodes

	// extract frequency number (low 11 bits of block_freq)
	uint32_t fnum = bitfield(cache.block_freq, 0, 11) << 1;

	// if there's a non-zero PM sensitivity, compute the adjustment
	uint32_t pm_sensitivity = ch_lfo_pm_sens(choffs);
	if (pm_sensitivity != 0)
	{
		// apply the phase adjustment based on the upper 7 bits
		// of FNUM and the PM depth parameters
		fnum += opn_lfo_pm_phase_adjustment(bitfield(cache.block_freq, 4, 7), pm_sensitivity, lfo_raw_pm);

		// keep fnum to 12 bits
		fnum &= 0xfff;
	}

	// apply block shift to compute phase step
	uint32_t block = bitfield(cache.block_freq, 11, 3);
	uint32_t phase_step = (fnum << block) >> 2;

	// apply detune based on the keycode
	phase_step += cache.detune;

	// clamp to 17 bits in case detune overflows
	// QUESTION: is this specific to the YM2612/3438?
	phase_step &= 0x1ffff;

	// apply frequency multiplier (which is cached as an x.1 value)
	return (phase_step * cache.multiple) >> 1;
}


//-------------------------------------------------
//  log_keyon - log a key-on event
//-------------------------------------------------

template<bool IsOpnA>
std::string opn_registers_base<IsOpnA>::log_keyon(uint32_t choffs, uint32_t opoffs)
{
	uint32_t chnum = (choffs & 3) + 3 * bitfield(choffs, 8);
	uint32_t opnum = (opoffs & 15) - ((opoffs & 15) / 4) + 12 * bitfield(opoffs, 8);

	uint32_t block_freq = ch_block_freq(choffs);
	if (multi_freq() && choffs == 2)
	{
		if (opoffs == 2)
			block_freq = multi_block_freq(1);
		else if (opoffs == 10)
			block_freq = multi_block_freq(2);
		else if (opoffs == 6)
			block_freq = multi_block_freq(0);
	}

	char buffer[256];
	int end = 0;

	end += snprintf(&buffer[end], sizeof(buffer) - end, "%u.%02u freq=%04X dt=%u fb=%u alg=%X mul=%X tl=%02X ksr=%u adsr=%02X/%02X/%02X/%X sl=%X",
		chnum, opnum,
		block_freq,
		op_detune(opoffs),
		ch_feedback(choffs),
		ch_algorithm(choffs),
		op_multiple(opoffs),
		op_total_level(opoffs),
		op_ksr(opoffs),
		op_attack_rate(opoffs),
		op_decay_rate(opoffs),
		op_sustain_rate(opoffs),
		op_release_rate(opoffs),
		op_sustain_level(opoffs));

	if (OUTPUTS > 1)
		end += snprintf(&buffer[end], sizeof(buffer) - end, " out=%c%c",
			ch_output_0(choffs) ? 'L' : '-',
			ch_output_1(choffs) ? 'R' : '-');
	if (op_ssg_eg_enable(opoffs))
		end += snprintf(&buffer[end], sizeof(buffer) - end, " ssg=%X", op_ssg_eg_mode(opoffs));
	bool am = (op_lfo_am_enable(opoffs) && ch_lfo_am_sens(choffs) != 0);
	if (am)
		end += snprintf(&buffer[end], sizeof(buffer) - end, " am=%u", ch_lfo_am_sens(choffs));
	bool pm = (ch_lfo_pm_sens(choffs) != 0);
	if (pm)
		end += snprintf(&buffer[end], sizeof(buffer) - end, " pm=%u", ch_lfo_pm_sens(choffs));
	if (am || pm)
		end += snprintf(&buffer[end], sizeof(buffer) - end, " lfo=%02X", lfo_rate());
	if (multi_freq() && choffs == 2)
		end += snprintf(&buffer[end], sizeof(buffer) - end, " multi=1");

	return buffer;
}


// openMSX uses the OPNA engine from a separate translation unit. Emit its
// public template methods explicitly; optimized builds may inline every
// call made by the chip wrappers in this file.
template class fm_engine_base<opna_registers>;

}
