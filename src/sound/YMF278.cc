// $Id$

// Based on ymf278b.c written by R. Belmont and O. Galibert

#include "YMF278.hh"
#include "ResampledSoundDevice.hh"
#include "Rom.hh"
#include "SimpleDebuggable.hh"
#include "MSXMotherBoard.hh"
#include "Clock.hh"
#include "MemBuffer.hh"
#include "serialize.hh"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace openmsx {

class DebugRegisters : public SimpleDebuggable
{
public:
	DebugRegisters(YMF278Impl& ymf278, MSXMotherBoard& motherBoard);
	virtual byte read(unsigned address);
	virtual void write(unsigned address, byte value, EmuTime::param time);
private:
	YMF278Impl& ymf278;
};

class DebugMemory : public SimpleDebuggable
{
public:
	DebugMemory(YMF278Impl& ymf278, MSXMotherBoard& motherBoard);
	virtual byte read(unsigned address);
	virtual void write(unsigned address, byte value);
private:
	YMF278Impl& ymf278;
};

class YMF278Slot
{
public:
	YMF278Slot();
	void reset();
	int compute_rate(int val) const;
	unsigned decay_rate(int num, int sample_rate);
	void envelope_next(int sample_rate);
	inline int compute_vib() const;
	inline int compute_am() const;
	void set_lfo(int newlfo);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

	unsigned startaddr;
	unsigned loopaddr;
	unsigned endaddr;
	unsigned step;		// fixed-point frequency step
	unsigned stepptr;	// fixed-point pointer into the sample
	unsigned pos;
	short sample1, sample2;

	int env_vol;

	int lfo_cnt;
	int lfo_step;
	int lfo_max;

	int DL;
	short wave;		// wavetable number
	short FN;		// f-number
	char OCT;		// octave
	char PRVB;		// pseudo-reverb
	char LD;		// level direct
	char TL;		// total level
	char pan;		// panpot
	char lfo;		// LFO
	char vib;		// vibrato
	char AM;		// AM level
	char AR;
	char D1R;
	char D2R;
	char RC;		// rate correction
	char RR;

	byte bits;		// width of the samples
	bool active;		// slot keyed on

	byte state;
	bool lfo_active;
};

class YMF278Impl : public ResampledSoundDevice
{
public:
	YMF278Impl(MSXMotherBoard& motherBoard, const std::string& name,
	       int ramSize, const XMLElement& config);
	virtual ~YMF278Impl();
	void clearRam();
	void reset(EmuTime::param time);
	void writeRegOPL4(byte reg, byte data, EmuTime::param time);
	byte readReg(byte reg, EmuTime::param time);
	byte peekReg(byte reg) const;
	byte readStatus(EmuTime::param time);
	byte peekStatus(EmuTime::param time) const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// SoundDevice
	virtual void generateChannels(int** bufs, unsigned num);

	void writeReg(byte reg, byte data, EmuTime::param time);
	byte readMem(unsigned address) const;
	void writeMem(unsigned address, byte value);
	short getSample(YMF278Slot& op);
	void advance();
	bool anyActive();
	void keyOnHelper(YMF278Slot& slot);

	MSXMotherBoard& motherBoard;
	friend class DebugRegisters;
	friend class DebugMemory;
	const std::auto_ptr<DebugRegisters> debugRegisters;
	const std::auto_ptr<DebugMemory>    debugMemory;

	/** The master clock, running at 33MHz. */
	typedef Clock<33868800> MasterClock;

	/** Required delay between register select and register read/write.
	  * TODO: Not used yet: register selection is done in MSXMoonSound class,
	  *       but it should be moved here.
	  */
	static const EmuDuration REG_SELECT_DELAY;
	/** Required delay after register write. */
	static const EmuDuration REG_WRITE_DELAY;
	/** Required delay after memory read. */
	static const EmuDuration MEM_READ_DELAY;
	/** Required delay after memory write (instead of register write delay). */
	static const EmuDuration MEM_WRITE_DELAY;
	/** Required delay after instrument load. */
	static const EmuDuration LOAD_DELAY;

	/** Time at which instrument loading is finished. */
	EmuTime loadTime;
	/** Time until which the YMF278 is busy. */
	EmuTime busyTime;

	YMF278Slot slots[24];

	/** Global envelope generator counter. */
	unsigned eg_cnt;

	int memadr;

	int fm_l, fm_r;
	int pcm_l, pcm_r;

	const std::auto_ptr<Rom> rom;
	MemBuffer<byte> ram;
	const unsigned endRom;
	const unsigned endRam;

	/** Precalculated attenuation values with some marging for
	  * enveloppe and pan levels.
	  */
	int volume[256 * 4];

	byte regs[256];
	char wavetblhdr;
	char memmode;
};

const EmuDuration YMF278Impl::REG_SELECT_DELAY = MasterClock::duration(88);
const EmuDuration YMF278Impl::REG_WRITE_DELAY = MasterClock::duration(88);
const EmuDuration YMF278Impl::MEM_READ_DELAY = MasterClock::duration(38);
const EmuDuration YMF278Impl::MEM_WRITE_DELAY = MasterClock::duration(28);
// 10000 ticks is approx 300us; exact delay is unknown.
const EmuDuration YMF278Impl::LOAD_DELAY = MasterClock::duration(10000);

const int EG_SH = 16; // 16.16 fixed point (EG timing)
const unsigned EG_TIMER_OVERFLOW = 1 << EG_SH;

// envelope output entries
const int ENV_BITS      = 10;
const int ENV_LEN       = 1 << ENV_BITS;
const double ENV_STEP   = 128.0 / ENV_LEN;
const int MAX_ATT_INDEX = (1 << (ENV_BITS - 1)) - 1; // 511
const int MIN_ATT_INDEX = 0;

// Envelope Generator phases
const int EG_ATT = 4;
const int EG_DEC = 3;
const int EG_SUS = 2;
const int EG_REL = 1;
const int EG_OFF = 0;

const int EG_REV = 5; // pseudo reverb
const int EG_DMP = 6; // damp

// Pan values, units are -3dB, i.e. 8.
const int pan_left[16]  = {
	0, 8, 16, 24, 32, 40, 48, 256, 256,   0,  0,  0,  0,  0,  0, 0
};
const int pan_right[16] = {
	0, 0,  0,  0,  0,  0,  0,   0, 256, 256, 48, 40, 32, 24, 16, 8
};

// Mixing levels, units are -3dB, and add some marging to avoid clipping
const int mix_level[8] = {
	8, 16, 24, 32, 40, 48, 56, 256
};

// decay level table (3dB per step)
// 0 - 15: 0, 3, 6, 9,12,15,18,21,24,27,30,33,36,39,42,93 (dB)
#define SC(db) unsigned(db * (2.0 / ENV_STEP))
const unsigned dl_tab[16] = {
 SC( 0), SC( 1), SC( 2), SC(3 ), SC(4 ), SC(5 ), SC(6 ), SC( 7),
 SC( 8), SC( 9), SC(10), SC(11), SC(12), SC(13), SC(14), SC(31)
};
#undef SC

const byte RATE_STEPS = 8;
const byte eg_inc[15 * RATE_STEPS] = {
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

// rate  0,    1,    2,    3,   4,   5,   6,  7,  8,  9,  10, 11, 12, 13, 14, 15
// shift 12,   11,   10,   9,   8,   7,   6,  5,  4,  3,  2,  1,  0,  0,  0,  0
// mask  4095, 2047, 1023, 511, 255, 127, 63, 31, 15, 7,  3,  1,  0,  0,  0,  0
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


// number of steps to take in quarter of lfo frequency
// TODO check if frequency matches real chip
#define O(a) int((EG_TIMER_OVERFLOW / a) / 6)
const int lfo_period[8] = {
	O(0.168), O(2.019), O(3.196), O(4.206),
	O(5.215), O(5.888), O(6.224), O(7.066)
};
#undef O


#define O(a) int(a * 65536)
const int vib_depth[8] = {
	O(0),	   O(3.378),  O(5.065),  O(6.750),
	O(10.114), O(20.170), O(40.106), O(79.307)
};
#undef O


#define SC(db) unsigned(db * (2.0 / ENV_STEP))
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

	lfo_active = false;
	lfo_cnt = lfo_step = 0;
	lfo_max = lfo_period[0];

	state = EG_OFF;
	active = false;

	// not strictly needed, but avoid UMR on savestate
	pos = sample1 = sample2 = 0;
}

int YMF278Slot::compute_rate(int val) const
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

int YMF278Slot::compute_vib() const
{
	return (((lfo_step << 8) / lfo_max) * vib_depth[int(vib)]) >> 24;
}


int YMF278Slot::compute_am() const
{
	if (lfo_active && AM) {
		return (((lfo_step << 8) / lfo_max) * am_depth[int(AM)]) >> 12;
	} else {
		return 0;
	}
}

void YMF278Slot::set_lfo(int newlfo)
{
	lfo_step = (((lfo_step << 8) / lfo_max) * newlfo) >> 8;
	lfo_cnt  = (((lfo_cnt  << 8) / lfo_max) * newlfo) >> 8;

	lfo = newlfo;
	lfo_max = lfo_period[int(lfo)];
}


void YMF278Impl::advance()
{
	eg_cnt++;
	for (int i = 0; i < 24; ++i) {
		YMF278Slot& op = slots[i];

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

short YMF278Impl::getSample(YMF278Slot& op)
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

bool YMF278Impl::anyActive()
{
	for (int i = 0; i < 24; ++i) {
		if (slots[i].active) {
			return true;
		}
	}
	return false;
}

void YMF278Impl::generateChannels(int** bufs, unsigned num)
{
	if (!anyActive()) {
		// TODO update internal state, even if muted
		// TODO also mute individual channels
		for (int i = 0; i < 24; ++i) {
			bufs[i] = 0;
		}
		return;
	}

	int vl = mix_level[pcm_l];
	int vr = mix_level[pcm_r];
	for (unsigned j = 0; j < num; ++j) {
		for (int i = 0; i < 24; ++i) {
			YMF278Slot& sl = slots[i];
			if (!sl.active) {
				//bufs[i][2 * j + 0] += 0;
				//bufs[i][2 * j + 1] += 0;
				continue;
			}

			short sample = (sl.sample1 * (0x10000 - sl.stepptr) +
			                sl.sample2 * sl.stepptr) >> 16;
			int vol = sl.TL + (sl.env_vol >> 2) + sl.compute_am();

			int volLeft  = vol + pan_left [int(sl.pan)] + vl;
			int volRight = vol + pan_right[int(sl.pan)] + vr;
			// TODO prob doesn't happen in real chip
			volLeft  = std::max(0, volLeft);
			volRight = std::max(0, volRight);

			bufs[i][2 * j + 0] += (sample * volume[volLeft] ) >> 14;
			bufs[i][2 * j + 1] += (sample * volume[volRight]) >> 14;

			if (sl.lfo_active && sl.vib) {
				int oct = sl.OCT;
				if (oct & 8) {
					oct |= -8;
				}
				oct += 5;
				unsigned step = (oct >= 0)
					? ((sl.FN | 1024) + sl.compute_vib()) << oct
					: ((sl.FN | 1024) + sl.compute_vib()) >> -oct;
				sl.stepptr += step;
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
		advance();
	}
}

void YMF278Impl::keyOnHelper(YMF278Slot& slot)
{
	slot.active = true;

	int oct = slot.OCT;
	if (oct & 8) {
		oct |= -8;
	}
	oct += 5;
	unsigned step = (oct >= 0)
		? (slot.FN | 1024) << oct
		: (slot.FN | 1024) >> -oct;
	slot.step = step;
	slot.state = EG_ATT;
	slot.stepptr = 0;
	slot.pos = 0;
	slot.sample1 = getSample(slot);
	slot.pos = 1;
	slot.sample2 = getSample(slot);
}

void YMF278Impl::writeRegOPL4(byte reg, byte data, EmuTime::param time)
{
	busyTime = time + REG_WRITE_DELAY;
	writeReg(reg, data, time);
}

void YMF278Impl::writeReg(byte reg, byte data, EmuTime::param time)
{
	updateStream(time); // TODO optimize only for regs that directly influence sound
	// Handle slot registers specifically
	if (reg >= 0x08 && reg <= 0xF7) {
		int snum = (reg - 8) % 24;
		YMF278Slot& slot = slots[snum];
		switch ((reg - 8) / 24) {
		case 0: {
			loadTime = time + LOAD_DELAY;

			slot.wave = (slot.wave & 0x100) | data;
			int base = (slot.wave < 384 || !wavetblhdr) ?
			           (slot.wave * 12) :
			           (wavetblhdr * 0x80000 + ((slot.wave - 384) * 12));
			byte buf[12];
			for (int i = 0; i < 12; ++i) {
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
			oct += 5;
			unsigned step = (oct >= 0)
				? (slot.FN | 1024) << oct
				: (slot.FN | 1024) >> -oct;
			slot.step = step;
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
			unsigned step = (oct >= 0)
				? (slot.FN | 1024) << oct
				: (slot.FN | 1024) >> -oct;
			slot.step = step;
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
				if (!(regs[reg] & 0x080)) {
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
			busyTime = time + MEM_WRITE_DELAY;
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

byte YMF278Impl::readReg(byte reg, EmuTime::param time)
{
	// no need to call updateStream(time)
	byte result;
	switch(reg) {
		case 2: // 3 upper bits are device ID
			result = (regs[2] & 0x1F) | 0x20;
			break;

		case 6: // Memory Data Register
			busyTime = time + MEM_READ_DELAY;
			result = readMem(memadr);
			memadr = (memadr + 1) & 0xFFFFFF;
			break;

		default:
			result = regs[reg];
			break;
	}
	return result;
}

byte YMF278Impl::peekReg(byte reg) const
{
	byte result;
	switch(reg) {
		case 2: // 3 upper bits are device ID
			result = (regs[2] & 0x1F) | 0x20;
			break;

		case 6: // Memory Data Register
			result = readMem(memadr);
			break;

		default:
			result = regs[reg];
			break;
	}
	return result;
}

byte YMF278Impl::readStatus(EmuTime::param time)
{
	// no need to call updateStream(time)
	return peekStatus(time);
}

byte YMF278Impl::peekStatus(EmuTime::param time) const
{
	byte result = 0;
	if (time < busyTime) result |= 0x01;
	if (time < loadTime) result |= 0x02;
	return result;
}

YMF278Impl::YMF278Impl(MSXMotherBoard& motherBoard_, const std::string& name,
                       int ramSize, const XMLElement& config)
	: ResampledSoundDevice(motherBoard_, name, "MoonSound wave-part",
	                       24, true)
	, motherBoard(motherBoard_)
	, debugRegisters(new DebugRegisters(*this, motherBoard))
	, debugMemory   (new DebugMemory   (*this, motherBoard))
	, loadTime(motherBoard.getCurrentTime())
	, busyTime(motherBoard.getCurrentTime())
	, rom(new Rom(motherBoard, name + " ROM", "rom", config))
	, ram(ramSize * 1024) // in kB
	, endRom(rom->getSize())
	, endRam(endRom + ram.size())
{
	memadr = 0; // avoid UMR

	setInputRate(44100);

	reset(motherBoard.getCurrentTime());
	registerSound(config);

	// Volume table, 1 = -0.375dB, 8 = -3dB, 256 = -96dB
	for (int i = 0; i < 256; ++i) {
		volume[i] = int(32768.0 * pow(2.0, (-0.375 / 6) * i));
	}
	for (int i = 256; i < 256 * 4; ++i) {
		volume[i] = 0;
	}
}

YMF278Impl::~YMF278Impl()
{
	unregisterSound();
}

void YMF278Impl::clearRam()
{
	memset(ram.data(), 0, ram.size());
}

void YMF278Impl::reset(EmuTime::param time)
{
	eg_cnt = 0;

	for (int i = 0; i < 24; ++i) {
		slots[i].reset();
	}
	for (int i = 255; i >= 0; --i) { // reverse order to avoid UMR
		writeRegOPL4(i, 0, time);
	}
	wavetblhdr = memmode = memadr = 0;
	fm_l = fm_r = pcm_l = pcm_r = 0;
	busyTime = time;
	loadTime = time;
}

byte YMF278Impl::readMem(unsigned address) const
{
	if (address < endRom) {
		return (*rom)[address];
	} else if (address < endRam) {
		return ram[address - endRom];
	} else {
		return 255; // TODO check
	}
}

void YMF278Impl::writeMem(unsigned address, byte value)
{
	if (address < endRom) {
		// can't write to ROM
	} else if (address < endRam) {
		ram[address - endRom] = value;
	} else {
		// can't write to unmapped memory
	}
}


template<typename Archive>
void YMF278Slot::serialize(Archive& ar, unsigned /*version*/)
{
	// TODO restore more state from registers
	ar.serialize("startaddr", startaddr);
	ar.serialize("loopaddr", loopaddr);
	ar.serialize("endaddr", endaddr);
	ar.serialize("step", step);
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
	ar.serialize("bits", bits);
	ar.serialize("active", active);
	ar.serialize("state", state);
	ar.serialize("lfo_active", lfo_active);

	// Older version also had "env_vol_step" and "env_vol_lim"
	// but those members were nowhere used, so removed those
	// in the current version (it's ok to remove members from the
	// savestate without updating the version number).
}

template<typename Archive>
void YMF278Impl::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("loadTime", loadTime);
	ar.serialize("busyTime", busyTime);
	ar.serialize("slots", slots);
	ar.serialize("eg_cnt", eg_cnt);
	ar.serialize_blob("ram", ram.data(), ram.size());
	ar.serialize_blob("registers", regs, sizeof(regs));

	// TODO restore more state from registers
	static const byte rewriteRegs[] = {
		2,       // wavetblhdr, memmode
		3, 4, 5, // memadr
		0xf8,    // fm_l, fm_r
		0xf9,    // pcm_l, pcm_r
	};
	if (ar.isLoader()) {
		EmuTime::param time = motherBoard.getCurrentTime();
		for (unsigned i = 0; i < sizeof(rewriteRegs); ++i) {
			byte reg = rewriteRegs[i];
			writeReg(reg, regs[reg], time);
		}
	}
}


// class DebugRegisters

DebugRegisters::DebugRegisters(YMF278Impl& ymf278_,
                                       MSXMotherBoard& motherBoard)
	: SimpleDebuggable(motherBoard, ymf278_.getName() + " regs",
	                   "OPL4 registers", 0x100)
	, ymf278(ymf278_)
{
}

byte DebugRegisters::read(unsigned address)
{
	return ymf278.peekReg(address);
}

void DebugRegisters::write(unsigned address, byte value, EmuTime::param time)
{
	ymf278.writeReg(address, value, time);
}


// class DebugMemory

DebugMemory::DebugMemory(YMF278Impl& ymf278_, MSXMotherBoard& motherBoard)
	: SimpleDebuggable(motherBoard, ymf278_.getName() + " mem",
	                   "OPL4 memory (includes both ROM and RAM)", 0x400000) // 4MB
	, ymf278(ymf278_)
{
}

byte DebugMemory::read(unsigned address)
{
	return ymf278.readMem(address);
}

void DebugMemory::write(unsigned address, byte value)
{
	ymf278.writeMem(address, value);
}


// class YMF278

YMF278::YMF278(MSXMotherBoard& motherBoard, const std::string& name,
               int ramSize, const XMLElement& config)
	: pimple(new YMF278Impl(motherBoard, name, ramSize, config))
{
}

YMF278::~YMF278()
{
}

void YMF278::clearRam()
{
	pimple->clearRam();
}

void YMF278::reset(EmuTime::param time)
{
	pimple->reset(time);
}

void YMF278::writeRegOPL4(byte reg, byte data, EmuTime::param time)
{
	pimple->writeRegOPL4(reg, data, time);
}

byte YMF278::readReg(byte reg, EmuTime::param time)
{
	return pimple->readReg(reg, time);
}

byte YMF278::peekReg(byte reg) const
{
	return pimple->peekReg(reg);
}

byte YMF278::readStatus(EmuTime::param time)
{
	return pimple->readStatus(time);
}

byte YMF278::peekStatus(EmuTime::param time) const
{
	return pimple->peekStatus(time);
}

template<typename Archive>
void YMF278::serialize(Archive& ar, unsigned version)
{
	pimple->serialize(ar, version);
}
INSTANTIATE_SERIALIZE_METHODS(YMF278);

} // namespace openmsx
