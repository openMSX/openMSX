// $Id$

#include "YMF278.hh"
#include <cmath>


namespace openmsx {

const int EG_SH = 16;	// 16.16 fixed point (EG timing)
const unsigned EG_TIMER_OVERFLOW = 1 << EG_SH;

// envelope output entries
const int ENV_BITS      = 10;
const int ENV_LEN       = 1 << ENV_BITS;
const double ENV_STEP   = 128.0 / ENV_LEN;
const int MAX_ATT_INDEX = (1 << (ENV_BITS - 1)) - 1; //511
const int MIN_ATT_INDEX = 0;

// Envelope Generator phases
const int EG_ATT = 4;
const int EG_DEC = 3;
const int EG_SUS = 2;
const int EG_REL = 1;
const int EG_OFF = 0;

const int EG_REV = 5;	//pseudo reverb
const int EG_DMP = 6;	//damp

// Pan values, units are -3dB, i.e. 8.
const int pan_left[16]  = {
	0, 8, 16, 24, 32, 40, 48, 256, 256,   0,  0,  0,  0,  0,  0, 0
};
const int pan_right[16] = {
	0, 0,  0,  0,  0,  0,  0,   0, 256, 256, 48, 40, 32, 24, 16, 8
};

// Mixing levels, units are -3dB, and add some marging to avoid clipping
const int mix_level[8] = {
	8, 16, 24, 32, 40, 48, 56, 0
};

// decay level table (3dB per step)
// 0 - 15: 0, 3, 6, 9,12,15,18,21,24,27,30,33,36,39,42,93 (dB)
#define SC(db) (unsigned)(db * (2.0 / ENV_STEP))
const unsigned dl_tab[16] = {
 SC( 0), SC( 1), SC( 2), SC(3 ), SC(4 ), SC(5 ), SC(6 ), SC( 7),
 SC( 8), SC( 9), SC(10), SC(11), SC(12), SC(13), SC(14), SC(31)
};
#undef SC

const byte RATE_STEPS = 8;
const byte eg_inc[15 * RATE_STEPS] = {
//cycle:0 1  2 3  4 5  6 7
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
const byte eg_rate_select[64] = {
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

//rate  0,    1,    2,    3,   4,   5,   6,  7,  8,  9,  10, 11, 12, 13, 14, 15
//shift 12,   11,   10,   9,   8,   7,   6,  5,  4,  3,  2,  1,  0,  0,  0,  0
//mask  4095, 2047, 1023, 511, 255, 127, 63, 31, 15, 7,  3,  1,  0,  0,  0,  0
#define O(a) (a)
const byte eg_rate_shift[64] = {
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


//number of steps to take in quarter of lfo frequency
//TODO check if frequency matches real chip
#define O(a) ((int)((EG_TIMER_OVERFLOW / a) / 6))
const int lfo_period[8] = {
	O(0.168), O(2.019), O(3.196), O(4.206),
	O(5.215), O(5.888), O(6.224), O(7.066)
};
#undef O


#define O(a) ((int)(a * 65536))
const int vib_depth[8] = {
	O(0),	   O(3.378),  O(5.065),  O(6.750),
	O(10.114), O(20.170), O(40.106), O(79.307)
};
#undef O


#define SC(db) (unsigned) (db * (2.0 / ENV_STEP))
const int am_depth[8] = {
	SC(0),	   SC(1.781), SC(2.906), SC(3.656),
	SC(4.406), SC(5.906), SC(7.406), SC(11.91)
};
#undef SC


YMF278Slot::YMF278Slot()
{
	reset();
}

void YMF278Slot::reset()
{
	wave = FN = OCT = PRVB = LD = TL = pan = lfo = vib = AM = 0;
	AR = D1R = DL = D2R = RC = RR = 0;
	step = stepptr = 0;
	bits = startaddr = loopaddr = endaddr = 0;
	env_vol = MAX_ATT_INDEX;
	//env_vol_step = env_vol_lim = 0;

	lfo_active = false;
	lfo_cnt = lfo_step = 0;
	lfo_max = lfo_period[0];

	state = EG_OFF;
	active = false;
}

int YMF278Slot::compute_rate(int val)
{
	if (val == 0) {
		return 0;
	} else if (val == 15) {
		return 63;
	}
	int res;
	if (RC != 15) {
		int oct = OCT;
		if (oct & 8) {
			oct |= -8;
		}
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

int YMF278Slot::compute_vib()
{
	return (((lfo_step << 8) / lfo_max) * vib_depth[(int)vib]) >> 24;
}


int YMF278Slot::compute_am()
{
	if (lfo_active && AM) {
		return (((lfo_step << 8) / lfo_max) * am_depth[(int)AM]) >> 12;
	} else {
		return 0;
	}
}

void YMF278Slot::set_lfo(int newlfo)
{
	lfo_step = (((lfo_step << 8) / lfo_max) * newlfo) >> 8;
	lfo_cnt  = (((lfo_cnt  << 8) / lfo_max) * newlfo) >> 8;

	lfo = newlfo;
	lfo_max = lfo_period[(int)lfo];
}


void YMF278::advance()
{
	eg_timer += eg_timer_add;
	while (eg_timer >= EG_TIMER_OVERFLOW) {
		eg_timer -= EG_TIMER_OVERFLOW;
		eg_cnt++;
		
		for (int i = 0; i < 24; i++) {
			YMF278Slot &op = slots[i];
			
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
			case EG_ATT: {	// attack phase
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
						op.state = EG_DEC;
					}
				}
				break;
			}
			case EG_DEC: {	// decay phase 
				byte rate = op.compute_rate(op.D1R);
				if (rate < 4) {
					break;
				}
				byte shift = eg_rate_shift[rate];
				if (!(eg_cnt & ((1 << shift) -1))) {
					byte select = eg_rate_select[rate];
					op.env_vol += eg_inc[select + ((eg_cnt >> shift) & 7)];
					
					if (((unsigned)op.env_vol > dl_tab[6]) && op.PRVB) {
						op.state = EG_REV;
					} else {
						if (op.env_vol >= op.DL) {
							op.state = EG_SUS;
						}
					}
				}
				break;
			}
			case EG_SUS: {	// sustain phase 
				byte rate = op.compute_rate(op.D2R);
				if (rate < 4) {
					break;
				}
				byte shift = eg_rate_shift[rate];
				if (!(eg_cnt & ((1 << shift) -1))) {
					byte select = eg_rate_select[rate];
					op.env_vol += eg_inc[select + ((eg_cnt >> shift) & 7)];
					
					if (((unsigned)op.env_vol > dl_tab[6]) && op.PRVB) {
						op.state = EG_REV;
					} else {
						if (op.env_vol >= MAX_ATT_INDEX) {
							op.env_vol = MAX_ATT_INDEX;
							op.active = false;
							checkMute();
						}
					}
				}
				break;
			}
			case EG_REL: {	// release phase 
				byte rate = op.compute_rate(op.RR);
				if (rate < 4) {
					break;
				}
				byte shift = eg_rate_shift[rate];
				if (!(eg_cnt & ((1 << shift) -1))) {
					byte select = eg_rate_select[rate];
					op.env_vol += eg_inc[select + ((eg_cnt >> shift) & 7)];
					
					if (((unsigned)op.env_vol > dl_tab[6]) && op.PRVB) {
						op.state = EG_REV;
					} else {
						if (op.env_vol >= MAX_ATT_INDEX) {
							op.env_vol = MAX_ATT_INDEX;
							op.active = false;
							checkMute();
						}
					}
				}
				break;
			}
			case EG_REV: {	//pseudo reverb
				//TODO improve env_vol update
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
						checkMute();
					}
				}
				break;
			}
			case EG_DMP: {	//damping
				//TODO improve env_vol update, damp is just fastest decay now
				byte rate = 56;
				byte shift = eg_rate_shift[rate];
				if (!(eg_cnt & ((1 << shift) - 1))) {
					byte select = eg_rate_select[rate];
					op.env_vol += eg_inc[select + ((eg_cnt >> shift) & 7)];
					
					if (op.env_vol >= MAX_ATT_INDEX) {
						op.env_vol = MAX_ATT_INDEX;
						op.active = false;
						checkMute();
					}
				}
				break;
			}
			case EG_OFF:
				// nothing
				break;
			
			default:
				assert(false);
				break;
			}
		}
	}
}

short YMF278::getSample(YMF278Slot &op)
{
	short sample;
	switch (op.bits) {
	case 0: {
		// 8 bit
		sample = readMem(op.startaddr + op.pos) << 8;
		break;
	}
	case 1: {
		// 12 bit
		int addr = op.startaddr + ((op.pos / 2) * 3);
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
		int addr = op.startaddr + (op.pos * 2);
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

void YMF278::checkMute()
{
	setInternalMute(!anyActive());
}

bool YMF278::anyActive()
{
	for (int i = 0; i < 24; i++) {
		if (slots[i].active) {
			return true;
		}
	}
	return false;
}

int* YMF278::updateBuffer(int length) throw()
{
	if (isInternalMuted()) {
		return NULL;
	}

	int vl = mix_level[pcm_l];
	int vr = mix_level[pcm_r];
	int *buf = buffer;
	while (length--) {
		int left = 0;
		int right = 0;
		for (int i = 0; i < 24; i++) {
			YMF278Slot &sl = slots[i];
			if (!sl.active) {
				continue;
			}

			short sample = (sl.sample1 * (0x10000 - sl.stepptr) +
			                sl.sample2 * sl.stepptr) >> 16;
			int vol = sl.TL + (sl.env_vol >> 2) + sl.compute_am();

			int volLeft  = vol + pan_left [(int)sl.pan] + vl;
			int volRight = vol + pan_right[(int)sl.pan] + vr;

			// TODO prob doesn't happen in real chip
			if (volLeft < 0) {
				volLeft = 0;
			}
			if (volRight < 0) {
				volRight = 0;
			}
			
			left  += (sample * volume[volLeft] ) >> 16;
			right += (sample * volume[volRight]) >> 16;
			
			if (sl.lfo_active && sl.vib) {
				int oct = sl.OCT;
				if (oct & 8) {
					oct |= -8;
				}
				int step = ((sl.FN | 1024) + sl.compute_vib()) << (oct + 5);
				sl.stepptr += (int)(step * freqbase);
			} else {
				sl.stepptr += sl.step;
			}

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
		*buf++ = left;
		*buf++ = right;

		advance();
	}
	return buffer;
}

void YMF278::keyOnHelper(YMF278Slot& slot)
{
	slot.active = true;
	setInternalMute(false);
	
	int oct = slot.OCT;
	if (oct & 8) {
		oct |= -8;
	}
	int step = (slot.FN | 1024) << (oct + 5);
	slot.step = (int)(step * freqbase);
	slot.state = EG_ATT;
	slot.stepptr = 0;
	slot.pos = 0;
	slot.sample1 = getSample(slot);
	slot.pos = 1;
	slot.sample2 = getSample(slot);
}

void YMF278::writeRegOPL4(byte reg, byte data, const EmuTime &time)
{
	if (BUSY_Time < time) {
		BUSY_Time = time;
	}
	BUSY_Time += 88;	// 88 cycles before next access
	
	// Handle slot registers specifically
	if (reg >= 0x08 && reg <= 0xF7) {
		int snum = (reg - 8) % 24;
		YMF278Slot& slot = slots[snum];
		switch ((reg - 8) / 24) {
		case 0: {
			if (LD_Time < time) {
				LD_Time = time;
			}
			LD_Time += 10000;	// approx 300us
			slot.wave = (slot.wave & 0x100) | data;
			int base = (slot.wave < 384 || !wavetblhdr) ?
			           (slot.wave * 12) :
			           (wavetblhdr * 0x80000 + ((slot.wave - 384) * 12));
			byte buf[12];
			for (int i = 0; i < 12; i++) {
				buf[i] = readMem(base + i);
			}
			slot.bits = (buf[0] & 0xC0) >> 6;
			slot.set_lfo((buf[7] >> 3) & 7);
			slot.vib  = buf[7] & 7;
			slot.AR   = buf[8] >> 4;
			slot.D1R  = buf[8] & 0xF;
			slot.DL   = dl_tab[buf[9] >> 4];
			slot.D2R  = buf[9] & 0xF;
			slot.RC   = buf[10] >> 4;
			slot.RR   = buf[10] & 0xF;
			slot.AM   = buf[11] & 7;
			slot.startaddr = buf[2] | (buf[1] << 8) | 
			                 ((buf[0] & 0x3F) << 16);
			slot.loopaddr = buf[4] + (buf[3] << 8);
			slot.endaddr  = (((buf[6] + (buf[5] << 8)) ^ 0xFFFF) + 1);
			if ((regs[reg + 4] & 0x080)) {
				keyOnHelper(slot);
			}
			break;
		}
		case 1: {
			slot.wave = (slot.wave & 0xFF) | ((data & 0x1) << 8);
			slot.FN = (slot.FN & 0x380) | (data >> 1);
			int oct = slot.OCT;
			if (oct & 8) {
				oct |= -8;
			}
			int step = (slot.FN | 1024) << (oct + 5);
			slot.step = (int)(step * freqbase);
			break;
		}
		case 2: {
			slot.FN = (slot.FN & 0x07F) | ((data & 0x07) << 7);
			slot.PRVB = ((data & 0x08) >> 3);
			slot.OCT =  ((data & 0xF0) >> 4);
			int oct = slot.OCT;
			if (oct & 8) {
				oct |= -8;
			}
			oct += 5;
			int step = (oct >= 0)
				? (slot.FN | 1024) << oct
				: (slot.FN | 1024) >> -oct;
			slot.step = (int)(step * freqbase);
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
			slot.pan = data & 0x0F;

			if (data & 0x020) {
				// LFO reset
				slot.lfo_active = false;
				slot.lfo_cnt = 0;
				slot.lfo_max = lfo_period[(int)slot.vib];
				slot.lfo_step = 0;
			} else {
				// LFO activate
				slot.lfo_active = true;
			}

			switch (data >> 6) {
			case 0:	//tone off, no damp
				if (slot.active && (slot.state != EG_REV) ) {
					slot.state = EG_REL;
				}
				break;
			case 1:	//tone off, damp
				slot.state = EG_DMP;
				break;
			case 2:	//tone on, no damp
				if (!(regs[reg] & 0x080)) {
					keyOnHelper(slot);
				}
				break;
			case 3:	//tone on, damp
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
		case 0x00:    	// TEST
		case 0x01:
			break;

		case 0x02:
			wavetblhdr = (data >> 2) & 0x7;
			memmode = data & 1;
			break;

		case 0x03:
			memadr = (memadr & 0x00FFFF) | (data << 16);
			break;

		case 0x04:
			memadr = (memadr & 0xFF00FF) | (data << 8);
			break;

		case 0x05:
			memadr = (memadr & 0xFFFF00) | data;
			break;

		case 0x06:  // memory data
			BUSY_Time += 28;	// 28 extra clock cycles
			writeMem(memadr, data);
			memadr = (memadr + 1) & 0xFFFFFF;
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

byte YMF278::readRegOPL4(byte reg, const EmuTime &time)
{
	if (BUSY_Time < time) {
		BUSY_Time = time;
	}
	BUSY_Time += 88;	// 88 cycles before next access
	
	byte result;
	switch(reg) {
		case 2: // 3 upper bits are device ID
			result = (regs[2] & 0x1F) | 0x20;
			break;
			
		case 6: // Memory Data Register
			BUSY_Time += 38;	// 38 extra clock cycles
			result = readMem(memadr);
			memadr = (memadr + 1) & 0xFFFFFF;
			break;

		default:
			result = regs[reg];
			break;
	}
	return result;
}

byte YMF278::readStatus(const EmuTime &time)
{
	byte result = 0;
	if (time < BUSY_Time) {
		result |= 0x01;
	}
	if (time < LD_Time) {
		result |= 0x02;
	}
	return result;
}

YMF278::YMF278(short volume, int ramSize, Device *config,
               const EmuTime& time)
	: rom(config, time)
{
	memadr = 0;	// avoid UMR
	endRom = rom.getSize();
	ramSize *= 1024;	// in kb
	ram = new byte[ramSize];
	endRam = endRom + ramSize;
	
	int bufSize = Mixer::instance().registerSound(this,
	                                               volume, Mixer::STEREO);
	buffer = new int[2 * bufSize];
	reset(time);
}

YMF278::~YMF278()
{
	Mixer::instance().unregisterSound(this);
	delete[] buffer;
	delete[] ram;
}

const string& YMF278::getName() const
{
	static const string name("MoonSoundWave");
	return name;
}

const string& YMF278::getDescription() const
{
	static const string desc("MoonSound wave-part");
	return desc;
}

void YMF278::reset(const EmuTime &time)
{
	eg_timer = 0;
	eg_cnt   = 0;

	for (int i = 0; i < 24; i++) {
		slots[i].reset();
	}
	for (int i = 255; i >= 0; i--) { // reverse order to avoid UMR
		writeRegOPL4(i, 0, time);
	}
	setInternalMute(true);
	wavetblhdr = memmode = memadr = 0;
	fm_l = fm_r = pcm_l = pcm_r = 0;
	BUSY_Time = time;
	LD_Time = time;
}

void YMF278::setSampleRate(int sampleRate)
{
	freqbase = 44100.0 / (double)sampleRate;
	eg_timer_add = (unsigned)((1 << EG_SH) * freqbase);
}

void YMF278::setInternalVolume(short newVolume)
{
	// Volume table, 1 = -0.375dB, 8 = -3dB, 256 = -96dB
	for (int i = 0; i < 256; i++) {
		volume[i] = (int)(4.0 * (double)newVolume * pow(2.0, (-0.375 / 6) * i));
	}
	for (int i = 256; i < 256 * 4; i++) {
		volume[i] = 0;
	}

}

byte YMF278::readMem(unsigned address)
{
	if (address < endRom) {
		return rom.read(address);
	} else if (address < endRam) {
		return ram[address - endRom];
	} else {
		return 255;	// TODO check
	}
}

void YMF278::writeMem(unsigned address, byte value)
{
	if (address < endRom) {
		// can't write to ROM
	} else if (address < endRam) {
		ram[address - endRom] = value;
	} else {
		// can't write to unmapped memory
	}
}

} // namespace openmsx

