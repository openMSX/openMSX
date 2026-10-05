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

namespace ymfm
{

//*********************************************************
//  GLOBAL TABLE LOOKUPS
//*********************************************************

//-------------------------------------------------
//  abs_sin_attenuation - given a sin (phase) input
//  where the range 0-2*PI is mapped onto 10 bits,
//  return the absolute value of sin(input),
//  logarithmically-adjusted and treated as an
//  attenuation value, in 4.8 fixed point format
//-------------------------------------------------

constexpr uint32_t abs_sin_attenuation(uint32_t input)
{
	// the values here are stored as 4.8 logarithmic values for 1/4 phase
	// this matches the internal format of the OPN chip, extracted from the die
	static constexpr uint16_t s_sin_table[256] =
	{
		0x859,0x6c3,0x607,0x58b,0x52e,0x4e4,0x4a6,0x471,0x443,0x41a,0x3f5,0x3d3,0x3b5,0x398,0x37e,0x365,
		0x34e,0x339,0x324,0x311,0x2ff,0x2ed,0x2dc,0x2cd,0x2bd,0x2af,0x2a0,0x293,0x286,0x279,0x26d,0x261,
		0x256,0x24b,0x240,0x236,0x22c,0x222,0x218,0x20f,0x206,0x1fd,0x1f5,0x1ec,0x1e4,0x1dc,0x1d4,0x1cd,
		0x1c5,0x1be,0x1b7,0x1b0,0x1a9,0x1a2,0x19b,0x195,0x18f,0x188,0x182,0x17c,0x177,0x171,0x16b,0x166,
		0x160,0x15b,0x155,0x150,0x14b,0x146,0x141,0x13c,0x137,0x133,0x12e,0x129,0x125,0x121,0x11c,0x118,
		0x114,0x10f,0x10b,0x107,0x103,0x0ff,0x0fb,0x0f8,0x0f4,0x0f0,0x0ec,0x0e9,0x0e5,0x0e2,0x0de,0x0db,
		0x0d7,0x0d4,0x0d1,0x0cd,0x0ca,0x0c7,0x0c4,0x0c1,0x0be,0x0bb,0x0b8,0x0b5,0x0b2,0x0af,0x0ac,0x0a9,
		0x0a7,0x0a4,0x0a1,0x09f,0x09c,0x099,0x097,0x094,0x092,0x08f,0x08d,0x08a,0x088,0x086,0x083,0x081,
		0x07f,0x07d,0x07a,0x078,0x076,0x074,0x072,0x070,0x06e,0x06c,0x06a,0x068,0x066,0x064,0x062,0x060,
		0x05e,0x05c,0x05b,0x059,0x057,0x055,0x053,0x052,0x050,0x04e,0x04d,0x04b,0x04a,0x048,0x046,0x045,
		0x043,0x042,0x040,0x03f,0x03e,0x03c,0x03b,0x039,0x038,0x037,0x035,0x034,0x033,0x031,0x030,0x02f,
		0x02e,0x02d,0x02b,0x02a,0x029,0x028,0x027,0x026,0x025,0x024,0x023,0x022,0x021,0x020,0x01f,0x01e,
		0x01d,0x01c,0x01b,0x01a,0x019,0x018,0x017,0x017,0x016,0x015,0x014,0x014,0x013,0x012,0x011,0x011,
		0x010,0x00f,0x00f,0x00e,0x00d,0x00d,0x00c,0x00c,0x00b,0x00a,0x00a,0x009,0x009,0x008,0x008,0x007,
		0x007,0x007,0x006,0x006,0x005,0x005,0x005,0x004,0x004,0x004,0x003,0x003,0x003,0x002,0x002,0x002,
		0x002,0x001,0x001,0x001,0x001,0x001,0x001,0x001,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000
	};

	// if the top bit is set, we're in the second half of the curve
	// which is a mirror image, so invert the index
	if (bitfield(input, 8))
		input = ~input;

	// return the value from the table
	return s_sin_table[input & 0xff];
}

// 10-bit phase, sign in bit 15. Built once from abs_sin_attenuation().
static constexpr auto s_waveform = generate_array<opna_registers::WAVEFORM_LENGTH>([](size_t index) {
	uint32_t i = uint32_t(index);
	return uint16_t(abs_sin_attenuation(i) | (bitfield(i, 9) << 15));
});


//-------------------------------------------------
//  attenuation_to_volume - given a 5.8 fixed point
//  logarithmic attenuation value, return a 13-bit
//  linear volume
//-------------------------------------------------

inline uint32_t attenuation_to_volume(uint32_t input)
{
	// the values here are 10-bit mantissas with an implied leading bit
	// this matches the internal format of the OPN chip, extracted from the die

	// as a nod to performance, the implicit 0x400 bit is pre-incorporated, and
	// the values are left-shifted by 2 so that a simple right shift is all that
	// is needed; also the order is reversed to save a NOT on the input
#define X(a) (((a) | 0x400) << 2)
	static uint16_t const s_power_table[256] =
	{
		X(0x3fa),X(0x3f5),X(0x3ef),X(0x3ea),X(0x3e4),X(0x3df),X(0x3da),X(0x3d4),
		X(0x3cf),X(0x3c9),X(0x3c4),X(0x3bf),X(0x3b9),X(0x3b4),X(0x3ae),X(0x3a9),
		X(0x3a4),X(0x39f),X(0x399),X(0x394),X(0x38f),X(0x38a),X(0x384),X(0x37f),
		X(0x37a),X(0x375),X(0x370),X(0x36a),X(0x365),X(0x360),X(0x35b),X(0x356),
		X(0x351),X(0x34c),X(0x347),X(0x342),X(0x33d),X(0x338),X(0x333),X(0x32e),
		X(0x329),X(0x324),X(0x31f),X(0x31a),X(0x315),X(0x310),X(0x30b),X(0x306),
		X(0x302),X(0x2fd),X(0x2f8),X(0x2f3),X(0x2ee),X(0x2e9),X(0x2e5),X(0x2e0),
		X(0x2db),X(0x2d6),X(0x2d2),X(0x2cd),X(0x2c8),X(0x2c4),X(0x2bf),X(0x2ba),
		X(0x2b5),X(0x2b1),X(0x2ac),X(0x2a8),X(0x2a3),X(0x29e),X(0x29a),X(0x295),
		X(0x291),X(0x28c),X(0x288),X(0x283),X(0x27f),X(0x27a),X(0x276),X(0x271),
		X(0x26d),X(0x268),X(0x264),X(0x25f),X(0x25b),X(0x257),X(0x252),X(0x24e),
		X(0x249),X(0x245),X(0x241),X(0x23c),X(0x238),X(0x234),X(0x230),X(0x22b),
		X(0x227),X(0x223),X(0x21e),X(0x21a),X(0x216),X(0x212),X(0x20e),X(0x209),
		X(0x205),X(0x201),X(0x1fd),X(0x1f9),X(0x1f5),X(0x1f0),X(0x1ec),X(0x1e8),
		X(0x1e4),X(0x1e0),X(0x1dc),X(0x1d8),X(0x1d4),X(0x1d0),X(0x1cc),X(0x1c8),
		X(0x1c4),X(0x1c0),X(0x1bc),X(0x1b8),X(0x1b4),X(0x1b0),X(0x1ac),X(0x1a8),
		X(0x1a4),X(0x1a0),X(0x19c),X(0x199),X(0x195),X(0x191),X(0x18d),X(0x189),
		X(0x185),X(0x181),X(0x17e),X(0x17a),X(0x176),X(0x172),X(0x16f),X(0x16b),
		X(0x167),X(0x163),X(0x160),X(0x15c),X(0x158),X(0x154),X(0x151),X(0x14d),
		X(0x149),X(0x146),X(0x142),X(0x13e),X(0x13b),X(0x137),X(0x134),X(0x130),
		X(0x12c),X(0x129),X(0x125),X(0x122),X(0x11e),X(0x11b),X(0x117),X(0x114),
		X(0x110),X(0x10c),X(0x109),X(0x106),X(0x102),X(0x0ff),X(0x0fb),X(0x0f8),
		X(0x0f4),X(0x0f1),X(0x0ed),X(0x0ea),X(0x0e7),X(0x0e3),X(0x0e0),X(0x0dc),
		X(0x0d9),X(0x0d6),X(0x0d2),X(0x0cf),X(0x0cc),X(0x0c8),X(0x0c5),X(0x0c2),
		X(0x0be),X(0x0bb),X(0x0b8),X(0x0b5),X(0x0b1),X(0x0ae),X(0x0ab),X(0x0a8),
		X(0x0a4),X(0x0a1),X(0x09e),X(0x09b),X(0x098),X(0x094),X(0x091),X(0x08e),
		X(0x08b),X(0x088),X(0x085),X(0x082),X(0x07e),X(0x07b),X(0x078),X(0x075),
		X(0x072),X(0x06f),X(0x06c),X(0x069),X(0x066),X(0x063),X(0x060),X(0x05d),
		X(0x05a),X(0x057),X(0x054),X(0x051),X(0x04e),X(0x04b),X(0x048),X(0x045),
		X(0x042),X(0x03f),X(0x03c),X(0x039),X(0x036),X(0x033),X(0x030),X(0x02d),
		X(0x02a),X(0x028),X(0x025),X(0x022),X(0x01f),X(0x01c),X(0x019),X(0x016),
		X(0x014),X(0x011),X(0x00e),X(0x00b),X(0x008),X(0x006),X(0x003),X(0x000)
	};
#undef X

	// look up the fractional part, then shift by the whole
	return s_power_table[input & 0xff] >> (input >> 8);
}


//-------------------------------------------------
//  attenuation_increment - given a 6-bit ADSR
//  rate value and a 3-bit stepping index,
//  return a 4-bit increment to the attenutaion
//  for this step (or for the attack case, the
//  fractional scale factor to decrease by)
//-------------------------------------------------

inline uint32_t attenuation_increment(uint32_t rate, uint32_t index)
{
	static uint32_t const s_increment_table[64] =
	{
		0x00000000, 0x00000000, 0x10101010, 0x10101010,  // 0-3    (0x00-0x03)
		0x10101010, 0x10101010, 0x11101110, 0x11101110,  // 4-7    (0x04-0x07)
		0x10101010, 0x10111010, 0x11101110, 0x11111110,  // 8-11   (0x08-0x0B)
		0x10101010, 0x10111010, 0x11101110, 0x11111110,  // 12-15  (0x0C-0x0F)
		0x10101010, 0x10111010, 0x11101110, 0x11111110,  // 16-19  (0x10-0x13)
		0x10101010, 0x10111010, 0x11101110, 0x11111110,  // 20-23  (0x14-0x17)
		0x10101010, 0x10111010, 0x11101110, 0x11111110,  // 24-27  (0x18-0x1B)
		0x10101010, 0x10111010, 0x11101110, 0x11111110,  // 28-31  (0x1C-0x1F)
		0x10101010, 0x10111010, 0x11101110, 0x11111110,  // 32-35  (0x20-0x23)
		0x10101010, 0x10111010, 0x11101110, 0x11111110,  // 36-39  (0x24-0x27)
		0x10101010, 0x10111010, 0x11101110, 0x11111110,  // 40-43  (0x28-0x2B)
		0x10101010, 0x10111010, 0x11101110, 0x11111110,  // 44-47  (0x2C-0x2F)
		0x11111111, 0x21112111, 0x21212121, 0x22212221,  // 48-51  (0x30-0x33)
		0x22222222, 0x42224222, 0x42424242, 0x44424442,  // 52-55  (0x34-0x37)
		0x44444444, 0x84448444, 0x84848484, 0x88848884,  // 56-59  (0x38-0x3B)
		0x88888888, 0x88888888, 0x88888888, 0x88888888   // 60-63  (0x3C-0x3F)
	};
	return bitfield(s_increment_table[rate], 4*index, 4);
}


//-------------------------------------------------
//  detune_adjustment - given a 5-bit key code
//  value and a 3-bit detune parameter, return a
//  6-bit signed phase displacement; this table
//  has been verified against Nuked's equations,
//  but the equations are rather complicated, so
//  we'll keep the simplicity of the table
//-------------------------------------------------

inline int32_t detune_adjustment(uint32_t detune, uint32_t keycode)
{
	static uint8_t const s_detune_adjustment[32][4] =
	{
		{ 0,  0,  1,  2 },  { 0,  0,  1,  2 },  { 0,  0,  1,  2 },  { 0,  0,  1,  2 },
		{ 0,  1,  2,  2 },  { 0,  1,  2,  3 },  { 0,  1,  2,  3 },  { 0,  1,  2,  3 },
		{ 0,  1,  2,  4 },  { 0,  1,  3,  4 },  { 0,  1,  3,  4 },  { 0,  1,  3,  5 },
		{ 0,  2,  4,  5 },  { 0,  2,  4,  6 },  { 0,  2,  4,  6 },  { 0,  2,  5,  7 },
		{ 0,  2,  5,  8 },  { 0,  3,  6,  8 },  { 0,  3,  6,  9 },  { 0,  3,  7, 10 },
		{ 0,  4,  8, 11 },  { 0,  4,  8, 12 },  { 0,  4,  9, 13 },  { 0,  5, 10, 14 },
		{ 0,  5, 11, 16 },  { 0,  6, 12, 17 },  { 0,  6, 13, 19 },  { 0,  7, 14, 20 },
		{ 0,  8, 16, 22 },  { 0,  8, 16, 22 },  { 0,  8, 16, 22 },  { 0,  8, 16, 22 }
	};
	int32_t result = s_detune_adjustment[keycode][detune & 3];
	return bitfield(detune, 2) ? -result : result;
}


//-------------------------------------------------
//  opn_lfo_pm_phase_adjustment - given the 7 most
//  significant frequency number bits, plus a 3-bit
//  PM depth value and a signed 5-bit raw PM value,
//  return a signed PM adjustment to the frequency;
//  algorithm written to match Nuked behavior
//-------------------------------------------------

inline int32_t opn_lfo_pm_phase_adjustment(uint32_t fnum_bits, uint32_t pm_sensitivity, int32_t lfo_raw_pm)
{
	// this table encodes 2 shift values to apply to the top 7 bits
	// of fnum; it is effectively a cheap multiply by a constant
	// value containing 0-2 bits
	static uint8_t const s_lfo_pm_shifts[8][8] =
	{
		{ 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77 },
		{ 0x77, 0x77, 0x77, 0x77, 0x72, 0x72, 0x72, 0x72 },
		{ 0x77, 0x77, 0x77, 0x72, 0x72, 0x72, 0x17, 0x17 },
		{ 0x77, 0x77, 0x72, 0x72, 0x17, 0x17, 0x12, 0x12 },
		{ 0x77, 0x77, 0x72, 0x17, 0x17, 0x17, 0x12, 0x07 },
		{ 0x77, 0x77, 0x17, 0x12, 0x07, 0x07, 0x02, 0x01 },
		{ 0x77, 0x77, 0x17, 0x12, 0x07, 0x07, 0x02, 0x01 },
		{ 0x77, 0x77, 0x17, 0x12, 0x07, 0x07, 0x02, 0x01 }
	};

	// look up the relevant shifts
	int32_t abs_pm = (lfo_raw_pm < 0) ? -lfo_raw_pm : lfo_raw_pm;
	uint32_t const shifts = s_lfo_pm_shifts[pm_sensitivity][bitfield(abs_pm, 0, 3)];

	// compute the adjustment
	int32_t adjust = (fnum_bits >> bitfield(shifts, 0, 4)) + (fnum_bits >> bitfield(shifts, 4, 4));
	if (pm_sensitivity > 5)
		adjust <<= pm_sensitivity - 5;
	adjust >>= 2;

	// every 16 cycles it inverts sign
	return (lfo_raw_pm < 0) ? -adjust : adjust;
}



//*********************************************************
//  FM OPERATOR
//*********************************************************

//-------------------------------------------------
//  fm_operator - constructor
//-------------------------------------------------

fm_operator::fm_operator(uint32_t opoffs) :
	m_opoffs(opoffs),
	m_phase(0),
	m_env_attenuation(0x3ff),
	m_env_state(EG_RELEASE),
	m_ssg_inverted(false),
	m_key_state(0),
	m_keyon_live(0),
	m_cache{}
{
}


//-------------------------------------------------
//  reset - reset the channel state
//-------------------------------------------------

void fm_operator::reset()
{
	// reset our data
	m_phase = 0;
	m_env_attenuation = 0x3ff;
	m_env_state = EG_RELEASE;
	m_ssg_inverted = 0;
	m_key_state = 0;
	m_keyon_live = 0;
}


//-------------------------------------------------
//  prepare - prepare for clocking
//-------------------------------------------------

bool fm_operator::prepare(opna_registers &regs, uint32_t choffs)
{
	// cache the data
	regs.cache_operator_data(choffs, m_opoffs, m_cache);

	// clock the key state
	clock_keystate(uint32_t(m_keyon_live != 0));
	m_keyon_live &= ~(1 << KEYON_CSM);

	// Only an enabled SSG-EG can invert. Turning it off mid-note leaves the
	// flag set, so it is cleared here, after start_release() has had its look
	// at the old value.
	if (!m_cache.ssg_eg_enable)
		m_ssg_inverted = false;

	// we're active until we're quiet after the release
	return (m_env_state != EG_RELEASE || m_env_attenuation < EG_QUIET);
}


//-------------------------------------------------
//  clock - master clocking function
//-------------------------------------------------

void fm_operator::clock(uint32_t env_counter, int32_t lfo_raw_pm)
{
	// clock the SSG-EG state (OPN/OPNA); prepare() cleared the inversion if
	// it is disabled
	if (m_cache.ssg_eg_enable)
		clock_ssg_eg_state();

	// clock the envelope if on an envelope cycle; env_counter is a x.2 value
	if (bitfield(env_counter, 0, 2) == 0)
		clock_envelope(env_counter >> 2);

	// clock the phase
	clock_phase(lfo_raw_pm);
}


//-------------------------------------------------
//  compute_volume - compute the 14-bit signed
//  volume of this operator, given a phase
//  modulation and an AM LFO offset
//-------------------------------------------------

int32_t fm_operator::compute_volume(uint32_t phase, uint32_t am_offset) const
{
	// the low 10 bits of phase represents a full 2*PI period over
	// the full sin wave

	// early out if the envelope is effectively off
	if (m_env_attenuation > EG_QUIET)
		return 0;

	// get the absolute value of the sin, as attenuation, as a 4.8 fixed point value
	uint32_t sin_attenuation = s_waveform[phase & (opna_registers::WAVEFORM_LENGTH - 1)];

	// get the attenuation from the evelope generator as a 4.6 value, shifted up to 4.8
	uint32_t env_attenuation = envelope_attenuation(am_offset) << 2;

	// combine into a 5.8 value, then convert from attenuation to 13-bit linear volume
	int32_t result = attenuation_to_volume((sin_attenuation & 0x7fff) + env_attenuation);

	// negate if in the negative part of the sin wave (sign bit gives 14 bits)
	return bitfield(sin_attenuation, 15) ? -result : result;
}


//-------------------------------------------------
//  keyonoff - signal a key on/off event
//-------------------------------------------------

void fm_operator::keyonoff(uint32_t on, keyon_type type)
{
	m_keyon_live = (m_keyon_live & ~(1 << int(type))) | (bitfield(on, 0) << int(type));
}


//-------------------------------------------------
//  start_attack - start the attack phase; called
//  when a keyon happens or when an SSG-EG cycle
//  is complete and restarts
//-------------------------------------------------

void fm_operator::start_attack(bool is_restart)
{
	// don't change anything if already in attack state
	if (m_env_state == EG_ATTACK)
		return;
	m_env_state = EG_ATTACK;

	// generally not inverted at start, except if SSG-EG is enabled and
	// one of the inverted modes is specified; leave this alone on a
	// restart, as it is managed by the clock_ssg_eg_state() code
	if (!is_restart)
		m_ssg_inverted = m_cache.ssg_eg_enable && bitfield(m_cache.ssg_eg_mode, 2);

	// reset the phase when we start an attack due to a key on
	// (but not when due to an SSG-EG restart except in certain cases
	// managed directly by the SSG-EG code)
	if (!is_restart)
		m_phase = 0;

	// if the attack rate >= 62 then immediately go to max attenuation
	if (m_cache.eg_rate[EG_ATTACK] >= 62)
		m_env_attenuation = 0;
}


//-------------------------------------------------
//  start_release - start the release phase;
//  called when a keyoff happens
//-------------------------------------------------

void fm_operator::start_release()
{
	// don't change anything if already in release state
	if (m_env_state >= EG_RELEASE)
		return;
	m_env_state = EG_RELEASE;

	// if attenuation if inverted due to SSG-EG, snap the inverted attenuation
	// as the starting point
	if (m_ssg_inverted)
	{
		m_env_attenuation = (0x200 - m_env_attenuation) & 0x3ff;
		m_ssg_inverted = false;
	}
}


//-------------------------------------------------
//  clock_keystate - clock the keystate to match
//  the incoming keystate
//-------------------------------------------------

void fm_operator::clock_keystate(uint32_t keystate)
{
	assert(keystate == 0 || keystate == 1);

	// has the key changed?
	if ((keystate ^ m_key_state) != 0)
	{
		m_key_state = keystate;

		// if the key has turned on, start the attack
		if (keystate != 0)
			start_attack();

		// otherwise, start the release
		else
			start_release();
	}
}


//-------------------------------------------------
//  clock_ssg_eg_state - clock the SSG-EG state;
//  should only be called if SSG-EG is enabled
//-------------------------------------------------

void fm_operator::clock_ssg_eg_state()
{
	// work only happens once the attenuation crosses above 0x200
	if (!bitfield(m_env_attenuation, 9))
		return;

	// 8 SSG-EG modes:
	//    000: repeat normally
	//    001: run once, hold low
	//    010: repeat, alternating between inverted/non-inverted
	//    011: run once, hold high
	//    100: inverted repeat normally
	//    101: inverted run once, hold low
	//    110: inverted repeat, alternating between inverted/non-inverted
	//    111: inverted run once, hold high
	uint32_t mode = m_cache.ssg_eg_mode;

	// hold modes (1/3/5/7)
	if (bitfield(mode, 0))
	{
		// set the inverted flag to the end state (0 for modes 1/7, 1 for modes 3/5)
		m_ssg_inverted = bitfield(mode, 2) ^ bitfield(mode, 1);

		// if holding, force the attenuation to the expected value once we're
		// past the attack phase
		if (m_env_state != EG_ATTACK)
			m_env_attenuation = m_ssg_inverted ? 0x200 : 0x3ff;
	}

	// continuous modes (0/2/4/6)
	else
	{
		// toggle invert in alternating mode (even in attack state)
		m_ssg_inverted ^= bitfield(mode, 1);

		// restart attack if in decay/sustain states
		if (m_env_state == EG_DECAY || m_env_state == EG_SUSTAIN)
			start_attack(true);

		// phase is reset to 0 in modes 0/4
		if (bitfield(mode, 1) == 0)
			m_phase = 0;
	}

	// in all modes, once we hit release state, attenuation is forced to maximum
	if (m_env_state == EG_RELEASE)
		m_env_attenuation = 0x3ff;
}


//-------------------------------------------------
//  clock_envelope - clock the envelope state
//  according to the given count
//-------------------------------------------------

void fm_operator::clock_envelope(uint32_t env_counter)
{
	// handle attack->decay transitions
	if (m_env_state == EG_ATTACK && m_env_attenuation == 0)
		m_env_state = EG_DECAY;

	// handle decay->sustain transitions; it is important to do this immediately
	// after the attack->decay transition above in the event that the sustain level
	// is set to 0 (in which case we will skip right to sustain without doing any
	// decay); as an example where this can be heard, check the cymbals sound
	// in channel 0 of shinobi's test mode sound #5
	if (m_env_state == EG_DECAY && m_env_attenuation >= m_cache.eg_sustain)
		m_env_state = EG_SUSTAIN;

	// fetch the appropriate 6-bit rate value from the cache
	uint32_t rate = m_cache.eg_rate[m_env_state];

	// compute the rate shift value; this is the shift needed to
	// apply to the env_counter such that it becomes a 5.11 fixed
	// point number
	uint32_t rate_shift = rate >> 2;
	env_counter <<= rate_shift;

	// see if the fractional part is 0; if not, it's not time to clock
	if (bitfield(env_counter, 0, 11) != 0)
		return;

	// determine the increment based on the non-fractional part of env_counter
	uint32_t relevant_bits = bitfield(env_counter, (rate_shift <= 11) ? 11 : rate_shift, 3);
	uint32_t increment = attenuation_increment(rate, relevant_bits);

	// attack is the only one that increases
	if (m_env_state == EG_ATTACK)
	{
		// glitch means that attack rates of 62/63 don't increment if
		// changed after the initial key on (where they are handled
		// specially); nukeykt confirms this happens on OPM, OPN, OPL/OPLL
		// at least so assuming it is true for everyone
		if (rate < 62)
			m_env_attenuation += (~m_env_attenuation * increment) >> 4;
	}

	// all other cases are similar
	else
	{
		// non-SSG-EG cases just apply the increment
		if (!m_cache.ssg_eg_enable)
			m_env_attenuation += increment;

		// SSG-EG only applies if less than mid-point, and then at 4x
		else if (m_env_attenuation < 0x200)
			m_env_attenuation += 4 * increment;

		// clamp the final attenuation
		if (m_env_attenuation >= 0x400)
			m_env_attenuation = 0x3ff;
	}
}


//-------------------------------------------------
//  clock_phase - clock the 10.10 phase value; the
//  OPN version of the logic has been verified
//  against the Nuked phase generator
//-------------------------------------------------

void fm_operator::clock_phase(int32_t lfo_raw_pm)
{
	// read from the cache, or recalculate if PM active
	uint32_t phase_step = m_cache.phase_step;
	if (phase_step == opdata_cache::PHASE_STEP_DYNAMIC)
		phase_step = opna_registers::compute_phase_step(m_cache, lfo_raw_pm);

	// finally apply the step to the current phase value
	m_phase += phase_step;
}


//-------------------------------------------------
//  envelope_attenuation - return the effective
//  attenuation of the envelope
//-------------------------------------------------

uint32_t fm_operator::envelope_attenuation(uint32_t am_offset) const
{
	uint32_t result = m_env_attenuation;

	// invert if necessary due to SSG-EG
	if (m_ssg_inverted)
		result = (0x200 - result) & 0x3ff;

	// add in LFO AM modulation
	if (m_cache.lfo_am_enable)
		result += am_offset;

	// add in total level from the cache
	result += m_cache.total_level;

	// clamp to max and return
	return std::min<uint32_t>(result, 0x3ff);
}



//*********************************************************
//  FM CHANNEL
//*********************************************************

//-------------------------------------------------
//  fm_channel - constructor
//-------------------------------------------------

fm_channel::fm_channel(uint32_t choffs, std::array<fm_operator *, 4> ops) :
	m_choffs(choffs),
	m_feedback{ 0, 0 },
	m_feedback_in(0),
	m_op(ops)
{
}


//-------------------------------------------------
//  reset - reset the channel state
//-------------------------------------------------

void fm_channel::reset()
{
	// reset our data
	m_feedback[0] = m_feedback[1] = 0;
	m_feedback_in = 0;
}


//-------------------------------------------------
//  keyonoff - signal key on/off to our operators
//-------------------------------------------------

void fm_channel::keyonoff(uint32_t states, keyon_type type)
{
	for (uint32_t opnum = 0; opnum < m_op.size(); opnum++)
		m_op[opnum]->keyonoff(bitfield(states, opnum), type);
}


//-------------------------------------------------
//  prepare - prepare for clocking
//-------------------------------------------------

bool fm_channel::prepare(opna_registers &regs)
{
	// prepare all operators and determine if any of them is active
	bool active = false;
	for (auto* op : m_op)
		if (op->prepare(regs, m_choffs))
			active = true;

	return active;
}


//-------------------------------------------------
//  clock - master clock of all operators
//-------------------------------------------------

void fm_channel::clock(uint32_t env_counter, int32_t lfo_raw_pm)
{
	// clock the feedback through
	m_feedback[0] = m_feedback[1];
	m_feedback[1] = m_feedback_in;

	for (auto* op : m_op)
		op->clock(env_counter, lfo_raw_pm);
}


//-------------------------------------------------
//  s_algorithm_ops - operator routing per algorithm
//
//  OPNA offers 8 connection algorithms for its 4 operators.
//
//  The operators are computed in order, with the inputs pulled from
//  an array of values (opout) that is populated as we go:
//     0 = 0
//     1 = O1
//     2 = O2
//     3 = O3
//     4 = (O4)
//     5 = O1+O2
//     6 = O1+O3
//     7 = O2+O3
//
//  Each row lists the opout[] index that feeds operators 2, 3 and 4,
//  and a mask of which of operators 1-3 also go to the output (operator 4
//  always does).
//-------------------------------------------------

#define ALGORITHM(op2in, op3in, op4in, op1out, op2out, op3out) \
	{ uint8_t(op2in), uint8_t(op3in), uint8_t(op4in), \
	  uint8_t((op1out) | ((op2out) << 1) | ((op3out) << 2)) }
static constexpr struct { uint8_t op2in, op3in, op4in, carrier_mask; } s_algorithm_ops[8] =
{
	ALGORITHM(1,2,3, 0,0,0),    //  0: O1 -> O2 -> O3 -> O4 -> out (O4)
	ALGORITHM(0,5,3, 0,0,0),    //  1: (O1 + O2) -> O3 -> O4 -> out (O4)
	ALGORITHM(0,2,6, 0,0,0),    //  2: (O1 + (O2 -> O3)) -> O4 -> out (O4)
	ALGORITHM(1,0,7, 0,0,0),    //  3: ((O1 -> O2) + O3) -> O4 -> out (O4)
	ALGORITHM(1,0,3, 0,1,0),    //  4: ((O1 -> O2) + (O3 -> O4)) -> out (O2+O4)
	ALGORITHM(1,1,1, 0,1,1),    //  5: ((O1 -> O2) + (O1 -> O3) + (O1 -> O4)) -> out (O2+O3+O4)
	ALGORITHM(1,0,0, 0,1,1),    //  6: ((O1 -> O2) + O3 + O4) -> out (O2+O3+O4)
	ALGORITHM(0,0,0, 1,1,1)     //  7: (O1 + O2 + O3 + O4) -> out (O1+O2+O3+O4)
};
#undef ALGORITHM


//-------------------------------------------------
//  make_output_plan - read the register fields that
//  hold for the whole buffer
//-------------------------------------------------

fm_channel::output_plan fm_channel::make_output_plan(const opna_registers &regs) const
{
	output_plan plan;
	auto const& alg = s_algorithm_ops[regs.ch_algorithm(m_choffs)];
	plan.op2in = alg.op2in;
	plan.op3in = alg.op3in;
	plan.op4in = alg.op4in;
	plan.carrier_mask = alg.carrier_mask;
	plan.feedback = uint8_t(regs.ch_feedback(m_choffs));

	plan.output_mask = 0;
	if (regs.ch_output_0(m_choffs))
		plan.output_mask |= 1;
	if (regs.ch_output_1(m_choffs))
		plan.output_mask |= 2;
	return plan;
}


//-------------------------------------------------
//  output_4op - combine 4 operators according to
//  the specified algorithm, returning a sum
//-------------------------------------------------

int32_t fm_channel::output_4op(const output_plan &plan, uint32_t am_offset) const
{
	// operator 1 has optional self-feedback
	int32_t opmod = 0;
	if (plan.feedback != 0)
		opmod = (m_feedback[0] + m_feedback[1]) >> (10 - plan.feedback);

	// compute the 14-bit volume/value of operator 1 and update the feedback
	int32_t op1value = m_feedback_in = m_op[0]->compute_volume(m_op[0]->phase() + opmod, am_offset);

	// now that the feedback has been computed, skip the rest if this channel
	// feeds no output; no need to do all this work for nothing
	if (plan.output_mask == 0)
		return 0;

	// populate the opout table
	int16_t opout[8];
	opout[0] = 0;
	opout[1] = op1value;

	// compute the 14-bit volume/value of operator 2
	opmod = opout[plan.op2in] >> 1;
	opout[2] = m_op[1]->compute_volume(m_op[1]->phase() + opmod, am_offset);
	opout[5] = opout[1] + opout[2];

	// compute the 14-bit volume/value of operator 3
	opmod = opout[plan.op3in] >> 1;
	opout[3] = m_op[2]->compute_volume(m_op[2]->phase() + opmod, am_offset);
	opout[6] = opout[1] + opout[3];
	opout[7] = opout[2] + opout[3];

	// compute the 14-bit volume/value of operator 4;
	// all algorithms consume OP4 output at a minimum
	opmod = opout[plan.op4in] >> 1;
	int32_t result = m_op[3]->compute_volume(m_op[3]->phase() + opmod, am_offset) >> OUTPUT_SHIFT;

	// optionally add OP1, OP2, OP3. compute_volume() cannot exceed the largest
	// power table entry, 8168, so even four unshifted carriers stay inside the
	// 16-bit range that the clamp here used to enforce.
	if (plan.carrier_mask & 1)
		result += opout[1] >> OUTPUT_SHIFT;
	if (plan.carrier_mask & 2)
		result += opout[2] >> OUTPUT_SHIFT;
	if (plan.carrier_mask & 4)
		result += opout[3] >> OUTPUT_SHIFT;

	return result;
}


//*********************************************************
//  FM ENGINE BASE
//*********************************************************

//-------------------------------------------------
//  fm_engine_base - constructor
//-------------------------------------------------

fm_engine_base::fm_engine_base(ymfm_interface &intf) :
	m_intf(intf),
	m_env_counter(0),
	m_status(0),
	m_clock_prescale(opna_registers::DEFAULT_PRESCALE),
	m_irq_mask(STATUS_TIMERA | STATUS_TIMERB),
	m_irq_state(0),
	m_timer_running{0,0},
	m_total_clocks(0),
	m_modified(false),
	m_operator(generate_array<OPERATORS>([](size_t opnum) {
		return fm_operator(opna_registers::operator_offset(uint32_t(opnum))); })),
	m_channel(generate_array<CHANNELS>([this](size_t chnum) {
		auto const& map = opna_registers::OPERATOR_MAP[chnum];
		return fm_channel(opna_registers::channel_offset(uint32_t(chnum)),
			std::array<fm_operator *, 4>{
				&m_operator[map[0]], &m_operator[map[1]],
				&m_operator[map[2]], &m_operator[map[3]] }); }))
{
}


//-------------------------------------------------
//  reset - reset the overall state
//-------------------------------------------------

void fm_engine_base::reset()
{
	// reset all status bits
	set_reset_status(0, 0xff);

	// register type-specific initialization
	m_regs.reset();

	// explicitly write to the mode register since it has side-effects
	// QUESTION: old cores initialize this to 0x30 -- who is right?
	write(opna_registers::REG_MODE, 0);

	// reset the channels
	for (auto &chan : m_channel)
		chan.reset();

	// reset the operators
	for (auto &op : m_operator)
		op.reset();
}


//-------------------------------------------------
//  synthesize_fm_channel - clock one channel across a stretch of samples
//  with no prepare() in between. Write selects whether output is mixed.
//-------------------------------------------------

template<bool Write, bool Lfo>
static void synthesize_fm_channel(fm_channel& channel, opna_registers& regs,
                                  float* buf [[maybe_unused]], unsigned num, uint32_t env)
{
	// Registers hold for the whole buffer, the AM shift among them. Without
	// the LFO walk the AM offset is constant as well, so it is read once too.
	fm_channel::output_plan plan;
	uint32_t lfoMaxCount = 0;
	uint32_t am_shift = 0;
	uint32_t am_offset = 0;
	bool panLeft = false;
	bool panRight = false;
	if constexpr (Lfo)
		lfoMaxCount = regs.lfo_max_count();
	if constexpr (Write) {
		plan = channel.make_output_plan(regs);
		panLeft = (plan.output_mask & 1) != 0;
		panRight = (plan.output_mask & 2) != 0;
		am_shift = regs.lfo_am_shift(channel.choffs());
		if constexpr (!Lfo)
			am_offset = regs.lfo_am_offset(am_shift);
	}

	auto tick = [&] {
		env = step_eg_counter(env);
		int32_t pm = 0;
		if constexpr (Lfo) {
			pm = regs.clock_lfo(lfoMaxCount);
			if constexpr (Write)
				am_offset = regs.lfo_am_offset(am_shift);
		}
		channel.clock(env, pm);
	};

	if constexpr (!Write) {
		for (unsigned index = 0; index < num; ++index)
			tick();
		return;
	}

	if (panLeft && panRight) {
		for (unsigned index = 0; index < num; ++index) {
			tick();
			float value = float(channel.output_4op(plan, am_offset));
			unsigned pos = index * 2;
			buf[pos + 0] += value;
			buf[pos + 1] += value;
		}
	} else if (panLeft) {
		for (unsigned index = 0; index < num; ++index) {
			tick();
			buf[index * 2] += float(channel.output_4op(plan, am_offset));
		}
	} else if (panRight) {
		for (unsigned index = 0; index < num; ++index) {
			tick();
			buf[index * 2 + 1] += float(channel.output_4op(plan, am_offset));
		}
	} else {
		for (unsigned index = 0; index < num; ++index) {
			tick();
			channel.output_4op(plan, am_offset);
		}
	}
}


//-------------------------------------------------
//  generate - one channel for the whole buffer, then the next
//-------------------------------------------------

void fm_engine_base::generate(std::span<float*, CHANNELS> buffers, unsigned num, uint32_t chanmask)
{
	static_assert(opna_registers::OUTPUTS == 2);
	static_assert(opna_registers::OPERATORS / opna_registers::CHANNELS == 4);

	// An empty buffer must not consume a pending key-on.
	if (num == 0)
		return;

	// Register writes and CSM key-on land before this call. A channel that is
	// quiet here cannot become active before the next generate(). The envelope
	// counter advances once per sample for ADPCM-A, even when no channel clocks.
	const uint32_t env0 = m_env_counter;
	const auto lfo0 = m_regs.save_lfo();
	const bool lfoEnabled = m_regs.lfo_enable() != 0;
	// Disabled LFO is a constant. One update serves every sample of the chunk.
	if (!lfoEnabled)
		m_regs.hold_disabled_lfo();

	bool lfoWalked = false;
	for (uint32_t chnum = 0; chnum < CHANNELS; ++chnum) {
		auto& channel = m_channel[chnum];
		// prepare() applies a new key and rebuilds the operator cache. With no
		// register or key change, the envelope state already decides the path.
		bool audible = m_modified ? channel.prepare(m_regs) : channel.audible();
		if (channel.finished()) {
			buffers[chnum] = nullptr;
			channel.quiesce_feedback(num);
			continue;
		}
		bool enabled = bitfield(chanmask, chnum) != 0;
		bool mix = audible && enabled;
		// Phase modulation matters only while the note is still running. AM is
		// read only when the channel is mixed.
		bool walkLfo = lfoEnabled && ((audible && m_regs.ch_lfo_pm_sens(channel.choffs()) != 0)
			|| (mix && m_regs.ch_lfo_am_sens(channel.choffs()) != 0));
		if (walkLfo) {
			m_regs.restore_lfo(lfo0);
			lfoWalked = true;
		}
		// Closed pan still clocks and updates operator-1 feedback, but adds
		// nothing, so the mixer can skip the buffer.
		if (!mix || (m_regs.ch_output_0(channel.choffs()) == 0 && m_regs.ch_output_1(channel.choffs()) == 0))
			buffers[chnum] = nullptr;
		if (mix && walkLfo)
			synthesize_fm_channel<true, true>(channel, m_regs, buffers[chnum], num, env0);
		else if (mix)
			synthesize_fm_channel<true, false>(channel, m_regs, buffers[chnum], num, env0);
		else if (walkLfo)
			synthesize_fm_channel<false, true>(channel, m_regs, nullptr, num, env0);
		else
			synthesize_fm_channel<false, false>(channel, m_regs, nullptr, num, env0);
	}

	m_env_counter = advance_eg_counter(env0, num);
	m_total_clocks = uint8_t(m_total_clocks + num);
	m_modified = false;
	if (lfoEnabled && !lfoWalked) {
		m_regs.restore_lfo(lfo0);
		const uint32_t lfoMaxCount = m_regs.lfo_max_count();
		for (unsigned i = 0; i < num; ++i)
			m_regs.clock_lfo(lfoMaxCount);
	}
}


//-------------------------------------------------
//  write - handle writes to the OPN registers
//-------------------------------------------------

void fm_engine_base::write(uint16_t regnum, uint8_t data)
{
	// special case: writes to the mode register can impact IRQs;
	// schedule these writes to ensure ordering with timers
	// Consumed by the next generate(): rebuild caches and apply key changes.
	m_modified = true;

	if (regnum == opna_registers::REG_MODE)
	{
		mode_write(data);
		return;
	}

	// most writes are passive, consumed only when needed
	uint32_t keyon_channel;
	uint32_t keyon_opmask;
	if (m_regs.write(regnum, data, keyon_channel, keyon_opmask))
	{
		// handle writes to the keyon register(s)
		if (keyon_channel < CHANNELS)
		{
			// normal channel on/off
			m_channel[keyon_channel].keyonoff(keyon_opmask, KEYON_NORMAL);
		}
	}
}


//-------------------------------------------------
//  status - return the current state of the
//  status flags
//-------------------------------------------------

uint8_t fm_engine_base::status() const
{
	return m_status & ~STATUS_BUSY;
}


//-------------------------------------------------
//  update_timer - update the state of the given
//  timer
//-------------------------------------------------

void fm_engine_base::update_timer(uint32_t tnum, uint32_t enable, int32_t delta_clocks)
{
	// if the timer is live, but not currently enabled, set the timer
	if (enable && !m_timer_running[tnum])
	{
		// period comes from the registers, and is different for each
		uint32_t period = (tnum == 0) ? (1024 - m_regs.timer_a_value()) : 16 * (256 - m_regs.timer_b_value());

		// caller can also specify a delta to account for other effects
		period += delta_clocks;

		// reset it
		m_intf.ymfm_set_timer(tnum, period * OPERATORS * m_clock_prescale);
		m_timer_running[tnum] = 1;
	}

	// if the timer is not live, ensure it is not enabled
	else if (!enable)
	{
		m_intf.ymfm_set_timer(tnum, -1);
		m_timer_running[tnum] = 0;
	}
}


//-------------------------------------------------
//  engine_timer_expired - timer has expired - signal
//  status and possibly IRQs
//-------------------------------------------------

void fm_engine_base::engine_timer_expired(uint32_t tnum)
{
	assert(tnum == 0 || tnum == 1);

	// update status
	if (tnum == 0 && m_regs.enable_timer_a())
		set_reset_status(STATUS_TIMERA, 0);
	else if (tnum == 1 && m_regs.enable_timer_b())
		set_reset_status(STATUS_TIMERB, 0);

	// if timer A fired in CSM mode, trigger CSM on channel 2, the only
	// channel OPNA keys from this timer
	if (tnum == 0 && m_regs.csm()) {
		m_modified = true;
		m_channel[2].keyonoff(0xf, KEYON_CSM);
	}

	// reset
	m_timer_running[tnum] = false;
	update_timer(tnum, 1, 0);
}


//-------------------------------------------------
//  check_interrupts - check the interrupt sources
//  for interrupts
//-------------------------------------------------

void fm_engine_base::check_interrupts()
{
	// update the state
	uint8_t old_state = m_irq_state;
	m_irq_state = ((m_status & m_irq_mask) != 0);

	// if changed, signal the new state
	if (old_state != m_irq_state)
		m_intf.ymfm_update_irq(m_irq_state != 0);
}


//-------------------------------------------------
//  mode_write - handle a mode register write
//-------------------------------------------------

void fm_engine_base::mode_write(uint8_t data)
{
	// actually write the mode register now
	uint32_t dummy1, dummy2;
	m_regs.write(opna_registers::REG_MODE, data, dummy1, dummy2);

	// reset timer status
	uint8_t reset_mask = 0;
	if (m_regs.reset_timer_b())
		reset_mask |= opna_registers::STATUS_TIMERB;
	if (m_regs.reset_timer_a())
		reset_mask |= opna_registers::STATUS_TIMERA;
	set_reset_status(0, reset_mask);

	// load timers; note that timer B gets a small negative adjustment because
	// the *16 multiplier is free-running, so the first tick of the clock
	// is a bit shorter
	update_timer(1, m_regs.load_timer_b(), -(m_total_clocks & 15));
	update_timer(0, m_regs.load_timer_a(), 0);
}

}
