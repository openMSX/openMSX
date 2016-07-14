// Based on ymf278b.c written by R. Belmont and O. Galibert

// This class doesn't model a full YMF278b chip. Instead it only models the
// wave part. The FM part in modeled in YMF262 (it's almost 100% compatible,
// the small differences are handled in YMF262). The status register and
// interaction with the FM registers (e.g. the NEW2 bit) is currently handled
// in the MSXMoonSound class.

#include "YMF278.hh"
#include "DeviceConfig.hh"
#include "MSXMotherBoard.hh"
#include "MSXException.hh"
#include "StringOp.hh"
#include "serialize.hh"
#include "likely.hh"
#include "outer.hh"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace openmsx {

static const int EG_SH = 16; // 16.16 fixed point (EG timing)
static const unsigned EG_TIMER_OVERFLOW = 1 << EG_SH;

// envelope output entries
static const int ENV_BITS      = 10;
static const int ENV_LEN       = 1 << ENV_BITS;
static const float ENV_STEP    = 128.0f / ENV_LEN;
static const int MAX_ATT_INDEX = (1 << (ENV_BITS - 1)) - 1; // 511
static const int MIN_ATT_INDEX = 0;

// Envelope Generator phases
static const int EG_ATT = 4;
static const int EG_DEC = 3;
static const int EG_SUS = 2;
static const int EG_REL = 1;
static const int EG_OFF = 0;

static const int EG_REV = 5; // pseudo reverb
static const int EG_DMP = 6; // damp

// Pan values, units are -3dB, i.e. 8.
static const int pan_left[16]  = {
	0, 8, 16, 24, 32, 40, 48, 256, 256,   0,  0,  0,  0,  0,  0, 0
};
static const int pan_right[16] = {
	0, 0,  0,  0,  0,  0,  0,   0, 256, 256, 48, 40, 32, 24, 16, 8
};

// Mixing levels, units are -3dB, and add some marging to avoid clipping
static const int mix_level[8] = {
	8, 16, 24, 32, 40, 48, 56, 256
};

// decay level table (3dB per step)
// 0 - 15: 0, 3, 6, 9,12,15,18,21,24,27,30,33,36,39,42,93 (dB)
#define SC(db) unsigned(db * (2.0f / ENV_STEP))
static const unsigned dl_tab[16] = {
 SC( 0), SC( 1), SC( 2), SC(3 ), SC(4 ), SC(5 ), SC(6 ), SC( 7),
 SC( 8), SC( 9), SC(10), SC(11), SC(12), SC(13), SC(14), SC(31)
};
#undef SC

static const byte RATE_STEPS = 8;
static const byte eg_inc[15 * RATE_STEPS] = {
//cycle:0  1   2  3   4  5   6  7
	0, 1,  0, 1,  0, 1,  0, 1, //  0  rates 00..12 0 (increment by 0 or 1)
	0, 1,  0, 1,  1, 1,  0, 1, //  1  rates 00..12 1
	0, 1,  1, 1,  0, 1,  1, 1, //  2  rates 00..12 2
	0, 1,  1, 1,  1, 1,  1, 1, //  3  rates 00..12 3

	1, 1,  1, 1,  1, 1,  1, 1, //  4  rate 13 0 (increment by 1)
	1, 1,  1, 2,  1, 1,  1, 2, //  5  rate 13 1
	1, 2,  1, 2,  1, 2,  1, 2, //  6  rate 13 2
	1, 2,  2, 2,  1, 2,  2, 2, //  7  rate 13 3

	2, 2,  2, 2,  2, 2,  2, 2, //  8  rate 14 0 (increment by 2)
	2, 2,  2, 4,  2, 2,  2, 4, //  9  rate 14 1
	2, 4,  2, 4,  2, 4,  2, 4, // 10  rate 14 2
	2, 4,  4, 4,  2, 4,  4, 4, // 11  rate 14 3

	4, 4,  4, 4,  4, 4,  4, 4, // 12  rates 15 0, 15 1, 15 2, 15 3 for decay
	8, 8,  8, 8,  8, 8,  8, 8, // 13  rates 15 0, 15 1, 15 2, 15 3 for attack (zero time)
	0, 0,  0, 0,  0, 0,  0, 0, // 14  infinity rates for attack and decay(s)
};

#define O(a) (a * RATE_STEPS)
static const byte eg_rate_select[64] = {
	O( 0),O( 1),O( 2),O( 3),
	O( 0),O( 1),O( 2),O( 3),
	O( 0),O( 1),O( 2),O( 3),
	O( 0),O( 1),O( 2),O( 3),
	O( 0),O( 1),O( 2),O( 3),
	O( 0),O( 1),O( 2),O( 3),
	O( 0),O( 1),O( 2),O( 3),
	O( 0),O( 1),O( 2),O( 3),
	O( 0),O( 1),O( 2),O( 3),
	O( 0),O( 1),O( 2),O( 3),
	O( 0),O( 1),O( 2),O( 3),
	O( 0),O( 1),O( 2),O( 3),
	O( 0),O( 1),O( 2),O( 3),
	O( 4),O( 5),O( 6),O( 7),
	O( 8),O( 9),O(10),O(11),
	O(12),O(12),O(12),O(12),
};
#undef O

// rate  0,    1,    2,    3,   4,   5,   6,  7,  8,  9,  10, 11, 12, 13, 14, 15
// shift 12,   11,   10,   9,   8,   7,   6,  5,  4,  3,  2,  1,  0,  0,  0,  0
// mask  4095, 2047, 1023, 511, 255, 127, 63, 31, 15, 7,  3,  1,  0,  0,  0,  0
#define O(a) (a)
static const byte eg_rate_shift[64] = {
	O(12),O(12),O(12),O(12),
	O(11),O(11),O(11),O(11),
	O(10),O(10),O(10),O(10),
	O( 9),O( 9),O( 9),O( 9),
	O( 8),O( 8),O( 8),O( 8),
	O( 7),O( 7),O( 7),O( 7),
	O( 6),O( 6),O( 6),O( 6),
	O( 5),O( 5),O( 5),O( 5),
	O( 4),O( 4),O( 4),O( 4),
	O( 3),O( 3),O( 3),O( 3),
	O( 2),O( 2),O( 2),O( 2),
	O( 1),O( 1),O( 1),O( 1),
	O( 0),O( 0),O( 0),O( 0),
	O( 0),O( 0),O( 0),O( 0),
	O( 0),O( 0),O( 0),O( 0),
	O( 0),O( 0),O( 0),O( 0),
};
#undef O


// number of steps to take in quarter of lfo frequency
// TODO check if frequency matches real chip
#define O(a) int((EG_TIMER_OVERFLOW / a) / 6)
static const int lfo_period[8] = {
	O(0.168f), O(2.019f), O(3.196f), O(4.206f),
	O(5.215f), O(5.888f), O(6.224f), O(7.066f)
};
#undef O


#define O(a) int(a * 65536)
static const int vib_depth[8] = {
	O( 0.0f  ), O( 3.378f), O( 5.065f), O( 6.750f),
	O(10.114f), O(20.170f), O(40.106f), O(79.307f)
};
#undef O


#define SC(db) int(db * (2.0f / ENV_STEP))
static const int am_depth[8] = {
	SC(0.0f  ), SC(1.781f), SC(2.906f), SC( 3.656f),
	SC(4.406f), SC(5.906f), SC(7.406f), SC(11.91f )
};
#undef SC


YMF278::Slot::Slot()
{
	reset();
}

// Sign extend a 4-bit value to int (32-bit)
// require: x in range [0..15]
static inline int sign_extend_4(int x)
{
	return (x ^ 8) - 8;
}

// Params: oct in [0 ..   15]
//         fn  in [0 .. 1023]
// We want to interpret oct as a signed 4-bit number and calculate
//    ((fn | 1024) + vib) << (5 + sign_extend_4(oct))
// Though in this formula the shift can go over a negative distance (in that
// case we should shift in the other direction).
static inline unsigned calcStep(unsigned oct, unsigned fn, unsigned vib = 0)
{
	oct ^= 8; // [0..15] -> [8..15][0..7] == sign_extend_4(x) + 8
	unsigned t = (fn + 1024 + vib) << oct; // use '+' iso '|' (generates slightly better code)
	return t >> 3; // was shifted 3 positions too far
}

void YMF278::Slot::reset()
{
	wave = FN = OCT = PRVB = LD = TL = pan = lfo = vib = AM = 0;
	AR = D1R = DL = D2R = RC = RR = 0;
	stepptr = 0;
	step = calcStep(OCT, FN);
	bits = startaddr = loopaddr = endaddr = 0;
	env_vol = MAX_ATT_INDEX;

	lfo_active = false;
	lfo_cnt = lfo_step = 0;
	lfo_max = lfo_period[0];

	state = EG_OFF;
	active = false;

	// not strictly needed, but avoid UMR on savestate
	pos = sample1 = sample2 = 0;
}

int YMF278::Slot::compute_rate(int val) const
{
	if (val == 0) {
		return 0;
	} else if (val == 15) {
		return 63;
	}
	int res;
	if (RC != 15) {
		// TODO it may be faster to store 'OCT' sign extended
		int oct = sign_extend_4(OCT);
		res = (oct + RC) * 2 + (FN & 0x200 ? 1 : 0) + val * 4;
	} else {
		res = val * 4;
	}
	if (res < 0) {
		res = 0;
	} else if (res > 63) {
		res = 63;
	}
	return res;
}

int YMF278::Slot::compute_vib() const
{
	return (((lfo_step << 8) / lfo_max) * vib_depth[int(vib)]) >> 24;
}


int YMF278::Slot::compute_am() const
{
	if (lfo_active && AM) {
		return (((lfo_step << 8) / lfo_max) * am_depth[int(AM)]) >> 12;
	} else {
		return 0;
	}
}

void YMF278::Slot::set_lfo(int newlfo)
{
	lfo_step = (((lfo_step << 8) / lfo_max) * newlfo) >> 8;
	lfo_cnt  = (((lfo_cnt  << 8) / lfo_max) * newlfo) >> 8;

	lfo = newlfo;
	lfo_max = lfo_period[int(lfo)];
}


void YMF278::advance()
{
	eg_cnt++;
	for (auto& op : slots) {
		if (op.lfo_active) {
			op.lfo_cnt++;
			if (op.lfo_cnt < op.lfo_max) {
				op.lfo_step++;
			} else if (op.lfo_cnt < (op.lfo_max * 3)) {
				op.lfo_step--;
			} else {
				op.lfo_step++;
				if (op.lfo_cnt == (op.lfo_max * 4)) {
					op.lfo_cnt = 0;
				}
			}
		}

		// Envelope Generator
		switch(op.state) {
		case EG_ATT: { // attack phase
			byte rate = op.compute_rate(op.AR);
			if (rate < 4) {
				break;
			}
			byte shift = eg_rate_shift[rate];
			if (!(eg_cnt & ((1 << shift) -1))) {
				byte select = eg_rate_select[rate];
				op.env_vol += (~op.env_vol * eg_inc[select + ((eg_cnt >> shift) & 7)]) >> 3;
				if (op.env_vol <= MIN_ATT_INDEX) {
					op.env_vol = MIN_ATT_INDEX;
					if (op.DL) {
						op.state = EG_DEC;
					} else {
						op.state = EG_SUS;
					}
				}
			}
			break;
		}
		case EG_DEC: { // decay phase
			byte rate = op.compute_rate(op.D1R);
			if (rate < 4) {
				break;
			}
			byte shift = eg_rate_shift[rate];
			if (!(eg_cnt & ((1 << shift) -1))) {
				byte select = eg_rate_select[rate];
				op.env_vol += eg_inc[select + ((eg_cnt >> shift) & 7)];

				if ((unsigned(op.env_vol) > dl_tab[6]) && op.PRVB) {
					op.state = EG_REV;
				} else {
					if (op.env_vol >= op.DL) {
						op.state = EG_SUS;
					}
				}
			}
			break;
		}
		case EG_SUS: { // sustain phase
			byte rate = op.compute_rate(op.D2R);
			if (rate < 4) {
				break;
			}
			byte shift = eg_rate_shift[rate];
			if (!(eg_cnt & ((1 << shift) -1))) {
				byte select = eg_rate_select[rate];
				op.env_vol += eg_inc[select + ((eg_cnt >> shift) & 7)];

				if ((unsigned(op.env_vol) > dl_tab[6]) && op.PRVB) {
					op.state = EG_REV;
				} else {
					if (op.env_vol >= MAX_ATT_INDEX) {
						op.env_vol = MAX_ATT_INDEX;
						op.active = false;
					}
				}
			}
			break;
		}
		case EG_REL: { // release phase
			byte rate = op.compute_rate(op.RR);
			if (rate < 4) {
				break;
			}
			byte shift = eg_rate_shift[rate];
			if (!(eg_cnt & ((1 << shift) -1))) {
				byte select = eg_rate_select[rate];
				op.env_vol += eg_inc[select + ((eg_cnt >> shift) & 7)];

				if ((unsigned(op.env_vol) > dl_tab[6]) && op.PRVB) {
					op.state = EG_REV;
				} else {
					if (op.env_vol >= MAX_ATT_INDEX) {
						op.env_vol = MAX_ATT_INDEX;
						op.active = false;
					}
				}
			}
			break;
		}
		case EG_REV: { // pseudo reverb
			// TODO improve env_vol update
			byte rate = op.compute_rate(5);
			//if (rate < 4) {
			//	break;
			//}
			byte shift = eg_rate_shift[rate];
			if (!(eg_cnt & ((1 << shift) - 1))) {
				byte select = eg_rate_select[rate];
				op.env_vol += eg_inc[select + ((eg_cnt >> shift) & 7)];

				if (op.env_vol >= MAX_ATT_INDEX) {
					op.env_vol = MAX_ATT_INDEX;
					op.active = false;
				}
			}
			break;
		}
		case EG_DMP: { // damping
			// TODO improve env_vol update, damp is just fastest decay now
			byte rate = 56;
			byte shift = eg_rate_shift[rate];
			if (!(eg_cnt & ((1 << shift) - 1))) {
				byte select = eg_rate_select[rate];
				op.env_vol += eg_inc[select + ((eg_cnt >> shift) & 7)];

				if (op.env_vol >= MAX_ATT_INDEX) {
					op.env_vol = MAX_ATT_INDEX;
					op.active = false;
				}
			}
			break;
		}
		case EG_OFF:
			// nothing
			break;

		default:
			UNREACHABLE;
		}
	}
}

int16_t YMF278::getSample(Slot& op)
{
	// TODO How does this behave when R#2 bit 0 = 1?
	//      As-if read returns 0xff? (Like for CPU memory reads.) Or is
	//      sound generation blocked at some higher level?
	int16_t sample;
	switch (op.bits) {
	case 0: {
		// 8 bit
		sample = readMem(op.startaddr + op.pos) << 8;
		break;
	}
	case 1: {
		// 12 bit
		unsigned addr = op.startaddr + ((op.pos / 2) * 3);
		if (op.pos & 1) {
			sample = readMem(addr + 2) << 8 |
				 ((readMem(addr + 1) << 4) & 0xF0);
		} else {
			sample = readMem(addr + 0) << 8 |
				 (readMem(addr + 1) & 0xF0);
		}
		break;
	}
	case 2: {
		// 16 bit
		unsigned addr = op.startaddr + (op.pos * 2);
		sample = (readMem(addr + 0) << 8) |
			 (readMem(addr + 1));
		break;
	}
	default:
		// TODO unspecified
		sample = 0;
	}
	return sample;
}

bool YMF278::anyActive()
{
	for (auto& op : slots) {
		if (op.active) return true;
	}
	return false;
}

void YMF278::generateChannels(int** bufs, unsigned num)
{
	if (!anyActive()) {
		// TODO update internal state, even if muted
		// TODO also mute individual channels
		for (int i = 0; i < 24; ++i) {
			bufs[i] = nullptr;
		}
		return;
	}

	int vl = mix_level[pcm_l];
	int vr = mix_level[pcm_r];
	for (unsigned j = 0; j < num; ++j) {
		for (int i = 0; i < 24; ++i) {
			auto& sl = slots[i];
			if (!sl.active) {
				//bufs[i][2 * j + 0] += 0;
				//bufs[i][2 * j + 1] += 0;
				continue;
			}

			int16_t sample = (sl.sample1 * (0x10000 - sl.stepptr) +
			                  sl.sample2 * sl.stepptr) >> 16;
			int vol = sl.TL + (sl.env_vol >> 2) + sl.compute_am();

			int volLeft  = vol + pan_left [int(sl.pan)] + vl;
			int volRight = vol + pan_right[int(sl.pan)] + vr;
			// TODO prob doesn't happen in real chip
			volLeft  = std::max(0, volLeft);
			volRight = std::max(0, volRight);

			bufs[i][2 * j + 0] += (sample * volume[volLeft] ) >> 14;
			bufs[i][2 * j + 1] += (sample * volume[volRight]) >> 14;

			unsigned step = (sl.lfo_active && sl.vib)
			              ? calcStep(sl.OCT, sl.FN, sl.compute_vib())
			              : sl.step;
			sl.stepptr += step;

			while (sl.stepptr >= 0x10000) {
				sl.stepptr -= 0x10000;
				sl.sample1 = sl.sample2;
				sl.pos++;
				if (sl.pos >= sl.endaddr) {
					sl.pos = sl.loopaddr;
				}
				sl.sample2 = getSample(sl);
			}
		}
		advance();
	}
}

void YMF278::keyOnHelper(YMF278::Slot& slot)
{
	slot.active = true;

	slot.state = EG_ATT;
	slot.stepptr = 0;
	slot.pos = 0;
	slot.sample1 = getSample(slot);
	slot.pos = 1;
	slot.sample2 = getSample(slot);
}

void YMF278::writeReg(byte reg, byte data, EmuTime::param time)
{
	updateStream(time); // TODO optimize only for regs that directly influence sound
	writeRegDirect(reg, data, time);
}

void YMF278::writeRegDirect(byte reg, byte data, EmuTime::param time)
{
	// Handle slot registers specifically
	if (reg >= 0x08 && reg <= 0xF7) {
		int snum = (reg - 8) % 24;
		auto& slot = slots[snum];
		switch ((reg - 8) / 24) {
		case 0: {
			slot.wave = (slot.wave & 0x100) | data;
			int wavetblhdr = (regs[2] >> 2) & 0x7;
			int base = (slot.wave < 384 || !wavetblhdr) ?
			           (slot.wave * 12) :
			           (wavetblhdr * 0x80000 + ((slot.wave - 384) * 12));
			byte buf[12];
			for (int i = 0; i < 12; ++i) {
				// TODO What if R#2 bit 0 = 1?
				//      See also getSample()
				buf[i] = readMem(base + i);
			}
			slot.bits = (buf[0] & 0xC0) >> 6;
			slot.startaddr = buf[2] | (buf[1] << 8) |
			                 ((buf[0] & 0x3F) << 16);
			slot.loopaddr = buf[4] + (buf[3] << 8);
			slot.endaddr  = (((buf[6] + (buf[5] << 8)) ^ 0xFFFF) + 1);
			for (int i = 7; i < 12; ++i) {
				// Verified on real YMF278:
				// After tone loading, if you read these
				// registers, their value actually has changed.
				writeRegDirect(8 + snum + (i - 2) * 24, buf[i], time);
			}
			if ((regs[reg + 0x60] & 0x080)) {
				keyOnHelper(slot);
			}
			break;
		}
		case 1: {
			slot.wave = (slot.wave & 0xFF) | ((data & 0x1) << 8);
			slot.FN = (slot.FN & 0x380) | (data >> 1);
			slot.step = calcStep(slot.OCT, slot.FN);
			break;
		}
		case 2: {
			slot.FN = (slot.FN & 0x07F) | ((data & 0x07) << 7);
			slot.PRVB = ((data & 0x08) >> 3);
			slot.OCT =  ((data & 0xF0) >> 4);
			slot.step = calcStep(slot.OCT, slot.FN);
			break;
		}
		case 3:
			slot.TL = data >> 1;
			slot.LD = data & 0x1;

			// TODO
			if (slot.LD) {
				// directly change volume
			} else {
				// interpolate volume
			}
			break;
		case 4:
			if (data & 0x10) {
				// output to DO1 pin:
				// this pin is not used in moonsound
				// we emulate this by muting the sound
				slot.pan = 8; // both left/right -inf dB
			} else {
				slot.pan = data & 0x0F;
			}

			if (data & 0x020) {
				// LFO reset
				slot.lfo_active = false;
				slot.lfo_cnt = 0;
				slot.lfo_max = lfo_period[int(slot.vib)];
				slot.lfo_step = 0;
			} else {
				// LFO activate
				slot.lfo_active = true;
			}

			switch (data >> 6) {
			case 0: // tone off, no damp
				if (slot.active && (slot.state != EG_REV) ) {
					slot.state = EG_REL;
				}
				break;
			case 2: // tone on, no damp
				// 'Life on Mars' bug fix:
				//    In case KEY=ON + DAMP (value 0xc0) and we reach
				//    'env_vol == MAX_ATT_INDEX' (-> slot.active = false)
				//    we didn't trigger keyOnHelper() because KEY didn't
				//    change OFF->ON. Fixed by also checking slot.state.
				// TODO real HW is probably simpler because EG_DMP is not
				// an actual state, nor is 'slot.active' stored.
				if (!slot.active || !(regs[reg] & 0x080)) {
					keyOnHelper(slot);
				}
				break;
			case 1: // tone off, damp
			case 3: // tone on,  damp
				slot.state = EG_DMP;
				break;
			}
			break;
		case 5:
			slot.vib = data & 0x7;
			slot.set_lfo((data >> 3) & 0x7);
			break;
		case 6:
			slot.AR  = data >> 4;
			slot.D1R = data & 0xF;
			break;
		case 7:
			slot.DL  = dl_tab[data >> 4];
			slot.D2R = data & 0xF;
			break;
		case 8:
			slot.RC = data >> 4;
			slot.RR = data & 0xF;
			break;
		case 9:
			slot.AM = data & 0x7;
			break;
		}
	} else {
		// All non-slot registers
		switch (reg) {
		case 0x00: // TEST
		case 0x01:
			break;

		case 0x02:
			// wave-table-header / memory-type / memory-access-mode
			// Simply store in regs[2]
			break;

		case 0x03:
			// Verified on real YMF278:
			// * Don't update the 'memadr' variable on writes to
			//   reg 3 and 4. Only store the value in the 'regs'
			//   array for later use.
			// * The upper 2 bits are not used to address the
			//   external memories (so from a HW pov they don't
			//   matter). But if you read back this register, the
			//   upper 2 bits always read as '0' (even if you wrote
			//   '1'). So we mask the bits here already.
			data &= 0x3F;
			break;

		case 0x04:
			// See reg 3.
			break;

		case 0x05:
			// Verified on real YMF278: (see above)
			// Only writes to reg 5 change the (full) 'memadr'.
			memadr = (regs[3] << 16) | (regs[4] << 8) | data;
			break;

		case 0x06:  // memory data
			if (regs[2] & 1) {
				writeMem(memadr, data);
				++memadr; // no need to mask (again) here
			} else {
				// Verified on real YMF278:
				//  - writes are ignored
				//  - memadr is NOT increased
			}
			break;

		case 0xF8:
			// TODO use these
			fm_l = data & 0x7;
			fm_r = (data >> 3) & 0x7;
			break;

		case 0xF9:
			pcm_l = data & 0x7;
			pcm_r = (data >> 3) & 0x7;
			break;
		}
	}

	regs[reg] = data;
}

byte YMF278::readReg(byte reg)
{
	// no need to call updateStream(time)
	byte result = peekReg(reg);
	if (reg == 6) {
		// Memory Data Register
		if (regs[2] & 1) {
			// Verified on real YMF278:
			// memadr is only increased when 'regs[2] & 1'
			++memadr; // no need to mask (again) here
		}
	}
	return result;
}

byte YMF278::peekReg(byte reg) const
{
	byte result;
	switch (reg) {
		case 2: // 3 upper bits are device ID
			result = (regs[2] & 0x1F) | 0x20;
			break;

		case 6: // Memory Data Register
			if (regs[2] & 1) {
				result = readMem(memadr);
			} else {
				// Verified on real YMF278
				result = 0xff;
			}
			break;

		default:
			result = regs[reg];
			break;
	}
	return result;
}

YMF278::YMF278(const std::string& name_, int ramSize_,
               const DeviceConfig& config)
	: ResampledSoundDevice(config.getMotherBoard(), name_, "MoonSound wave-part",
	                       24, true)
	, motherBoard(config.getMotherBoard())
	, debugRegisters(motherBoard, getName())
	, debugMemory   (motherBoard, getName())
	, rom(getName() + " ROM", "rom", config)
	, ramSize(ramSize_ * 1024) // in kB
	, ram(ramSize)
{
	if (rom.getSize() != 0x200000) { // 2MB
		throw MSXException(
			"Wrong ROM for MoonSound (YMF278). The ROM (usually "
			"called yrw801.rom) should have a size of exactly 2MB.");
	}
	if ((ramSize_ !=    0) &&  //   -     -
	    (ramSize_ !=  128) &&  // 128kB   -
	    (ramSize_ !=  256) &&  // 128kB  128kB
	    (ramSize_ !=  512) &&  // 512kB   -
	    (ramSize_ !=  640) &&  // 512kB  128kB
	    (ramSize_ != 1024) &&  // 512kB  512kB
	    (ramSize_ != 2048)) {  // 512kB  512kB  512kB  512kB
		throw MSXException(StringOp::Builder() <<
			"Wrong sampleram size for MoonSound (YMF278). "
			"Got " << ramSize_ << ", but must be one of "
			"0, 128, 256, 512, 640, 1024 or 2048.");
	}

	memadr = 0; // avoid UMR

	setInputRate(44100);

	reset(motherBoard.getCurrentTime());
	registerSound(config);

	// Volume table, 1 = -0.375dB, 8 = -3dB, 256 = -96dB
	for (int i = 0; i < 256; ++i) {
		volume[i] = int(32768.0 * exp2((-0.375 / 6) * i));
	}
	for (int i = 256; i < 256 * 4; ++i) {
		volume[i] = 0;
	}
}

YMF278::~YMF278()
{
	unregisterSound();
}

void YMF278::clearRam()
{
	memset(ram.data(), 0, ramSize);
}

void YMF278::reset(EmuTime::param time)
{
	updateStream(time);

	eg_cnt = 0;

	for (auto& op : slots) {
		op.reset();
	}
	regs[2] = 0; // avoid UMR
	for (int i = 255; i >= 0; --i) { // reverse order to avoid UMR
		writeRegDirect(i, 0, time);
	}
	memadr = 0;
	fm_l = fm_r = pcm_l = pcm_r = 0;
}

// This routine translates an address from the (upper) MoonSound address space
// to an address inside the (linearized) SRAM address space.
//
// The following info is based on measurements on a real MoonSound (v2.0)
// PCB. This PCB can have several possible SRAM configurations:
//   128kB:
//    1 SRAM chip of 128kB, chip enable (/CE) of this SRAM chip is connected to
//    the 1Y0 output of a 74LS139 (2-to-4 decoder). The enable input of the
//    74LS139 is connected to YMF278 pin /MCS6 and the 74LS139 1B:1A inputs are
//    connected to YMF278 pins MA18:MA17. So the SRAM is selected when /MC6 is
//    active and MA18:MA17 == 0:0.
//   256kB:
//    2 SRAM chips of 128kB. First one connected as above. Second one has /CE
//    connected to 74LS139 pin 1Y1. So SRAM2 is selected when /MSC6 is active
//    and MA18:MA17 == 0:1.
//   512kB:
//    1 SRAM chip of 512kB, /CE connected to /MCS6
//   640kB:
//    1 SRAM chip of 512kB, /CE connected to /MCS6
//    1 SRAM chip of 128kB, /CE connected to /MCS7.
//      (This means SRAM2 is potentially mirrored over a 512kB region)
//  1024kB:
//    1 SRAM chip of 512kB, /CE connected to /MCS6
//    1 SRAM chip of 512kB, /CE connected to /MCS7
//  2048kB:
//    1 SRAM chip of 512kB, /CE connected to /MCS6
//    1 SRAM chip of 512kB, /CE connected to /MCS7
//    1 SRAM chip of 512kB, /CE connected to /MCS8
//    1 SRAM chip of 512kB, /CE connected to /MCS9
//      This configuration is not so easy to create on the v2.0 PCB. So it's
//      very rare.
//
// So the /MCS6 and /MCS7 (and /MCS8 and /MCS9 in case of 2048kB) signals are
// used to select the different SRAM chips. The meaning of these signals
// depends on the 'memory access mode'. This mode can be changed at run-time
// via bit 1 in register 2. The following table indicates for which regions
// these signals are active (normally MoonSound should be used with mode=0):
//              mode=0              mode=1
//  /MCS6   0x200000-0x27FFFF   0x380000-0x39FFFF
//  /MCS7   0x280000-0x2FFFFF   0x3A0000-0x3BFFFF
//  /MCS8   0x300000-0x37FFFF   0x3C0000-0x3DFFFF
//  /MCS9   0x380000-0x3FFFFF   0x3E0000-0x3FFFFF
//
// (For completeness) MoonSound also has 2MB ROM (YRW801), /CE of this ROM is
// connected to YMF278 /MCS0. In both mode=0 and mode=1 this signal is active
// for the region 0x000000-0x1FFFFF. (But this routine does not handle ROM).
unsigned YMF278::getRamAddress(unsigned addr) const
{
	addr -= 0x200000; // RAM starts at 0x200000
	if (unlikely(regs[2] & 2)) {
		// Normally MoonSound is used in 'memory access mode = 0'. But
		// in the rare case that mode=1 we adjust the address.
		if ((0x180000 <= addr) && (addr <= 0x1FFFFF)) {
			addr -= 0x180000;
			switch (addr & 0x060000) {
			case 0x000000: // [0x380000-0x39FFFF]
				// 1st 128kB of SRAM1
				break;
			case 0x020000: // [0x3A0000-0x3BFFFF]
				if (ramSize == 256 * 1024) {
					// 2nd 128kB SRAM chip
				} else {
					// 2nd block of 128kB in SRAM2
					// In case of 512+128, we use mirroring
					addr += 0x080000;
				}
				break;
			case 0x040000: // [0x3C0000-0x3DFFFF]
				// 3rd 128kB block in SRAM3
				addr += 0x100000;
				break;
			case 0x060000: // [0x3EFFFF-0x3FFFFF]
				// 4th 128kB block in SRAM4
				addr += 0x180000;
				break;
			}
		} else {
			addr = unsigned(-1); // unmapped
		}
	}
	if (ramSize == 640 * 1024) {
		// Verified on real MoonSound cartridge (v2.0): In case of
		// 640kB (1x512kB + 1x128kB), the 128kB SRAM chip is 4 times
		// visible. None of the other SRAM configurations show similar
		// mirroring (because the others are powers of two).
		if (addr > 0x080000) {
			addr &= ~0x060000;
		}
	}
	return addr;
}

byte YMF278::readMem(unsigned address) const
{
	// Verified on real YMF278: address space wraps at 4MB.
	address &= 0x3FFFFF;
	if (address < 0x200000) {
		// ROM connected to /MCS0
		return rom[address];
	} else {
		unsigned ramAddr = getRamAddress(address);
		if (ramAddr < ramSize) {
			return ram[ramAddr];
		} else {
			// unmapped region
			return 255; // TODO check
		}
	}
}

void YMF278::writeMem(unsigned address, byte value)
{
	address &= 0x3FFFFF;
	if (address < 0x200000) {
		// can't write to ROM
	} else {
		unsigned ramAddr = getRamAddress(address);
		if (ramAddr < ramSize) {
			ram[ramAddr] = value;
		} else {
			// can't write to unmapped memory
		}
	}
}

// version 1: initial version, some variables were saved as char
// version 2: serialization framework was fixed to save/load chars as numbers
//            but for backwards compatibility we still load old savestates as
//            characters
// version 3: 'step' is no longer stored (it is recalculated)
template<typename Archive>
void YMF278::Slot::serialize(Archive& ar, unsigned version)
{
	// TODO restore more state from registers
	ar.serialize("startaddr", startaddr);
	ar.serialize("loopaddr", loopaddr);
	ar.serialize("endaddr", endaddr);
	ar.serialize("stepptr", stepptr);
	ar.serialize("pos", pos);
	ar.serialize("sample1", sample1);
	ar.serialize("sample2", sample2);
	ar.serialize("env_vol", env_vol);
	ar.serialize("lfo_cnt", lfo_cnt);
	ar.serialize("lfo_step", lfo_step);
	ar.serialize("lfo_max", lfo_max);
	ar.serialize("DL", DL);
	ar.serialize("wave", wave);
	ar.serialize("FN", FN);
	if (ar.versionAtLeast(version, 2)) {
		ar.serialize("OCT", OCT);
		ar.serialize("PRVB", PRVB);
		ar.serialize("LD", LD);
		ar.serialize("TL", TL);
		ar.serialize("pan", pan);
		ar.serialize("lfo", lfo);
		ar.serialize("vib", vib);
		ar.serialize("AM", AM);
		ar.serialize("AR", AR);
		ar.serialize("D1R", D1R);
		ar.serialize("D2R", D2R);
		ar.serialize("RC", RC);
		ar.serialize("RR", RR);
	} else {
		ar.serializeChar("OCT", OCT);
		ar.serializeChar("PRVB", PRVB);
		ar.serializeChar("LD", LD);
		ar.serializeChar("TL", TL);
		ar.serializeChar("pan", pan);
		ar.serializeChar("lfo", lfo);
		ar.serializeChar("vib", vib);
		ar.serializeChar("AM", AM);
		ar.serializeChar("AR", AR);
		ar.serializeChar("D1R", D1R);
		ar.serializeChar("D2R", D2R);
		ar.serializeChar("RC", RC);
		ar.serializeChar("RR", RR);
	}
	ar.serialize("bits", bits);
	ar.serialize("active", active);
	ar.serialize("state", state);
	ar.serialize("lfo_active", lfo_active);

	// Recalculate redundant state
	if (ar.isLoader()) {
		step = calcStep(OCT, FN);
	}

	// This old comment is NOT completely true:
	//    Older version also had "env_vol_step" and "env_vol_lim" but those
	//    members were nowhere used, so removed those in the current
	//    version (it's ok to remove members from the savestate without
	//    updating the version number).
	// When you remove member variables without increasing the version
	// number, new openMSX executables can still read old savestates. And
	// if you try to load a new savestate in an old openMSX version you do
	// get a (cryptic) error message. But if the version number is
	// increased the error message is much clearer.
}

// version 1: initial version
// version 2: loadTime and busyTime moved to MSXMoonSound class
// version 3: memadr cannot be restored from register values
template<typename Archive>
void YMF278::serialize(Archive& ar, unsigned version)
{
	ar.serialize("slots", slots);
	ar.serialize("eg_cnt", eg_cnt);
	ar.serialize_blob("ram", ram.data(), ramSize);
	ar.serialize_blob("registers", regs, sizeof(regs));
	if (ar.versionAtLeast(version, 3)) { // must come after 'regs'
		ar.serialize("memadr", memadr);
	} else {
		assert(ar.isLoader());
		// Old formats didn't store 'memadr' so we also can't magically
		// restore the correct value. The best we can do is restore the
		// last set address.
		regs[3] &= 0x3F; // mask upper two bits
		memadr = (regs[3] << 16) | (regs[4] << 8) | regs[5];
	}

	// TODO restore more state from registers
	static const byte rewriteRegs[] = {
		0xf8,    // fm_l, fm_r
		0xf9,    // pcm_l, pcm_r
	};
	if (ar.isLoader()) {
		EmuTime::param time = motherBoard.getCurrentTime();
		for (auto r : rewriteRegs) {
			writeRegDirect(r, regs[r], time);
		}
	}
}
INSTANTIATE_SERIALIZE_METHODS(YMF278);


// class DebugRegisters

YMF278::DebugRegisters::DebugRegisters(MSXMotherBoard& motherBoard_,
                                       const std::string& name_)
	: SimpleDebuggable(motherBoard_, name_ + " regs",
	                   "OPL4 registers", 0x100)
{
}

byte YMF278::DebugRegisters::read(unsigned address)
{
	auto& ymf278 = OUTER(YMF278, debugRegisters);
	return ymf278.peekReg(address);
}

void YMF278::DebugRegisters::write(unsigned address, byte value, EmuTime::param time)
{
	auto& ymf278 = OUTER(YMF278, debugRegisters);
	ymf278.writeReg(address, value, time);
}


// class DebugMemory

YMF278::DebugMemory::DebugMemory(MSXMotherBoard& motherBoard_,
                                 const std::string& name_)
	: SimpleDebuggable(motherBoard_, name_ + " mem",
	                   "OPL4 memory (includes both ROM and RAM)", 0x400000) // 4MB
{
}

byte YMF278::DebugMemory::read(unsigned address)
{
	auto& ymf278 = OUTER(YMF278, debugMemory);
	return ymf278.readMem(address);
}

void YMF278::DebugMemory::write(unsigned address, byte value)
{
	auto& ymf278 = OUTER(YMF278, debugMemory);
	ymf278.writeMem(address, value);
}

} // namespace openmsx
