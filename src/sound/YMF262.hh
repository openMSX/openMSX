// $Id$

/*
 * File: ymf262.c - software implementation of YMF262
 *                  FM sound generator type OPL3
 *
 * Copyright (C) 2003 Jarek Burczynski
 *
 * Version 0.1
 *
 *
 * Revision History:
 *
 * 03-03-2003: initial release
 *  - thanks to Olivier Galibert and Chris Hardy for YMF262 and YAC512 chips
 *  - thanks to Stiletto for the datasheets
 *
 *
 *
 * differences between OPL2 and OPL3 not documented in Yamaha datahasheets:
 * - sinus table is a little different: the negative part is off by one...
 *
 * - in order to enable selection of four different waveforms on OPL2
 *   one must set bit 5 in register 0x01(test).
 *   on OPL3 this bit is ignored and 4-waveform select works *always*.
 *   (Don't confuse this with OPL3's 8-waveform select.)
 *
 * - Envelope Generator: all 15 x rates take zero time on OPL3
 *   (on OPL2 15 0 and 15 1 rates take some time while 15 2 and 15 3 rates
 *   take zero time)
 *
 * - channel calculations: output of operator 1 is in perfect sync with
 *   output of operator 2 on OPL3; on OPL and OPL2 output of operator 1
 *   is always delayed by one sample compared to output of operator 2
 *
 *
 * differences between OPL2 and OPL3 shown in datasheets:
 * - YMF262 does not support CSM mode
 */

#ifndef __YMF262_HH__
#define __YMF262_HH__

#include "openmsx.hh"
#include "SoundDevice.hh"
#include "IRQHelper.hh"
#include "EmuTimer.hh"


namespace openmsx {

class YMF262Slot
{
public:
	YMF262Slot();
	inline int volume_calc(byte LFO_AM);
	inline void FM_KEYON(byte key_set);
	inline void FM_KEYOFF(byte key_clr);

	byte ar;	// attack rate: AR<<2
	byte dr;	// decay rate:  DR<<2
	byte rr;	// release rate:RR<<2
	byte KSR;	// key scale rate
	byte ksl;	// keyscale level
	byte ksr;	// key scale rate: kcode>>KSR
	byte mul;	// multiple: mul_tab[ML]

	// Phase Generator 
	unsigned Cnt;	// frequency counter
	unsigned Incr;	// frequency counter step
	byte FB;	// feedback shift value
	int* connect;	// slot output pointer
	int op1_out[2];	// slot1 output for feedback
	byte CON;	// connection (algorithm) type

	// Envelope Generator 
	byte eg_type;	// percussive/non-percussive mode 
	byte state;	// phase type
	unsigned TL;	// total level: TL << 2
	int TLL;	// adjusted now TL
	int volume;	// envelope counter
	int sl;		// sustain level: sl_tab[SL]

	unsigned eg_m_ar;// (attack state)
	byte eg_sh_ar;	// (attack state)
	byte eg_sel_ar;	// (attack state)
	unsigned eg_m_dr;// (decay state)
	byte eg_sh_dr;	// (decay state)
	byte eg_sel_dr;	// (decay state)
	unsigned eg_m_rr;// (release state)
	byte eg_sh_rr;	// (release state)
	byte eg_sel_rr;	// (release state)

	byte key;	// 0 = KEY OFF, >0 = KEY ON

	// LFO 
	byte  AMmask;	// LFO Amplitude Modulation enable mask 
	byte vib;	// LFO Phase Modulation enable flag (active high)

	// waveform select 
	byte waveform_number;
	unsigned int wavetable;
};

class YMF262Channel
{
public:
	YMF262Channel();
	void chan_calc(byte LFO_AM);
	void chan_calc_ext(byte LFO_AM);
	void CALC_FCSLOT(YMF262Slot &slot);

	YMF262Slot slots[2];

	int block_fnum;	// block+fnum
	int fc;		// Freq. Increment base
	int ksl_base;	// KeyScaleLevel Base step
	byte kcode;	// key code (for key scaling)

	// there are 12 2-operator channels which can be combined in pairs
	// to form six 4-operator channel, they are:
	//  0 and 3,
	//  1 and 4,
	//  2 and 5,
	//  9 and 12,
	//  10 and 13,
	//  11 and 14
	byte extended;	// set to 1 if this channel forms up a 4op channel with another channel(only used by first of pair of channels, ie 0,1,2 and 9,10,11) 
};

class YMF262 : private SoundDevice, private EmuTimerCallback, private Debuggable
{
public:
	YMF262(short volume, const EmuTime& time);
	virtual ~YMF262();
	
	virtual void reset(const EmuTime& time);
	void writeReg(int r, byte v, const EmuTime& time);
	byte readReg(int reg);
	byte readStatus();
	
private:
	// SoundDevice
	virtual const string& getName() const;
	virtual const string& getDescription() const;
	virtual void setVolume(int volume);
	virtual void setSampleRate(int sampleRate);
	virtual int* updateBuffer(int length);

	// Debuggable
	virtual unsigned getSize() const;
	//virtual const string& getDescription() const;  // also in SoundDevice!!
	virtual byte read(unsigned address);
	virtual void write(unsigned address, byte value);
	
	void callback(byte flag);

	void writeRegForce(int r, byte v, const EmuTime& time);
	void init_tables(void);
	void setStatus(byte flag);
	void resetStatus(byte flag);
	void changeStatusMask(byte flag);
	void advance_lfo();
	void advance();
	void chan_calc_rhythm(bool noise);
	void set_mul(byte sl, byte v);
	void set_ksl_tl(byte sl, byte v);
	void set_ar_dr(byte sl, byte v);
	void set_sl_rr(byte sl, byte v);
	void update_channels(YMF262Channel &ch);
	void checkMute();
	bool checkMuteHelper();

	byte reg[512];
	YMF262Channel channels[18];	// OPL3 chips have 18 channels

	unsigned pan[18*4];		// channels output masks (0xffffffff = enable); 4 masks per one channel 

	unsigned eg_cnt;		// global envelope generator counter
	unsigned eg_timer;		// global envelope generator counter works at frequency = chipclock/288 (288=8*36) 
	unsigned eg_timer_add;		// step of eg_timer

	unsigned fn_tab[1024];		// fnumber->increment counter

	// LFO 
	byte LFO_AM;
	byte LFO_PM;
	
	byte lfo_am_depth;
	byte lfo_pm_depth_range;
	unsigned lfo_am_cnt;
	unsigned lfo_am_inc;
	unsigned lfo_pm_cnt;
	unsigned lfo_pm_inc;

	unsigned noise_rng;		// 23 bit noise shift register
	unsigned noise_p;		// current noise 'phase'
	unsigned noise_f;		// current noise period

	bool OPL3_mode;			// OPL3 extension enable flag
	byte rhythm;			// Rhythm mode
	byte nts;			// NTS (note select)

	byte status;			// status flag
	byte status2;
	byte statusMask;		// status mask
	IRQHelper irq;

	// Bitmask for register 0x04 
	static const int R04_ST1          = 0x01;	// Timer1 Start
	static const int R04_ST2          = 0x02;	// Timer2 Start
	static const int R04_MASK_T2      = 0x20;	// Mask Timer2 flag 
	static const int R04_MASK_T1      = 0x40;	// Mask Timer1 flag 
	static const int R04_IRQ_RESET    = 0x80;	// IRQ RESET 

	// Bitmask for status register 
	static const int STATUS_T2      = R04_MASK_T2;
	static const int STATUS_T1      = R04_MASK_T1;
	// Timers
	EmuTimer<12500, STATUS_T1> timer1;	//  80us
	EmuTimer< 3125, STATUS_T2> timer2;	// 320us

	int chanout[18];		// 18 channels 
	int* buffer;
	int maxVolume;
};

} // namespace openmsx

#endif
