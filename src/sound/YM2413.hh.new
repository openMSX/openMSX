// $Id$

#ifndef __YM2413_HH__
#define __YM2413_HH__

#include "openmsx.hh"
#include "SoundDevice.hh"
#include "Mixer.hh"

class EmuTime;


class Slot
{
	public:
		Slot();

		inline int volume_calc(byte LFO_AM);
		inline void KEY_ON (byte key_set);
		inline void KEY_OFF(byte key_clr);

		byte ar;	// attack rate: AR<<2
		byte dr;	// decay rate:  DR<<2
		byte rr;	// release rate:RR<<2
		byte KSR;	// key scale rate
		byte ksl;	// keyscale level
		byte ksr;	// key scale rate: kcode>>KSR
		byte mul;	// multiple: mul_tab[ML]

		// Phase Generator
		int phase;	// frequency counter
		int freq;	// frequency counter step
		byte fb_shift;	// feedback shift value
		int op1_out[2];	// slot1 output for feedback

		// Envelope Generator
		byte eg_type;	// percussive/nonpercussive mode
		byte state;	// phase type
		int TL;		// total level: TL << 2
		int TLL;	// adjusted now TL
		int volume;	// envelope counter
		int sl;		// sustain level: sl_tab[SL]

		byte eg_sh_dp;	// (dump state)
		byte eg_sel_dp;	// (dump state)
		byte eg_sh_ar;	// (attack state)
		byte eg_sel_ar;	// (attack state)
		byte eg_sh_dr;	// (decay state)
		byte eg_sel_dr;	// (decay state)
		byte eg_sh_rr;	// (release state for non-perc.)
		byte eg_sel_rr;	// (release state for non-perc.)
		byte eg_sh_rs;	// (release state for perc.mode)
		byte eg_sel_rs;	// (release state for perc.mode)

		byte key;	// 0 = KEY OFF, >0 = KEY ON

		// LFO
		byte AMmask;	// LFO Amplitude Modulation enable mask
		byte vib;	// LFO Phase Modulation enable flag (active high)

		int wavetable;	// waveform select
};

class Channel
{
	public:
		Channel();
		inline int chan_calc(byte LFO_AM);
		inline void CALC_FCSLOT(Slot *slot);

		Slot slots[2];
		// phase generator state
		int block_fnum;	// block+fnum
		int fc;		// Freq. freqement base
		int ksl_base;	// KeyScaleLevel Base step
		byte kcode;	// key code (for key scaling)
		byte sus;	// sus on/off (release speed in percussive mode)
};

class YM2413 : public SoundDevice
{
	public:
		YM2413(short volume, const EmuTime &time,
		       const Mixer::ChannelMode mode);
		virtual ~YM2413();
		
		void reset(const EmuTime &time);
		void writeReg(byte r, byte v, const EmuTime &time);
		
		virtual void setInternalVolume(short newVolume);
		virtual int* updateBuffer(int length);
		virtual void setSampleRate(int sampleRate);

	private:
		void checkMute();
		bool checkMuteHelper();
		
		void init_tables();
		
		inline void advance_lfo();
		inline void advance();
		inline int rhythm_calc(bool noise);

		inline void set_mul(byte slot, byte v);
		inline void set_ksl_tl(byte chan, byte v);
		inline void set_ksl_wave_fb(byte chan, byte v);
		inline void set_ar_dr(byte slot, byte v);
		inline void set_sl_rr(byte slot, byte v);
		void load_instrument(byte chan, byte slot, byte* inst);
		void update_instrument_zero(byte r);
		void setRhythmMode(bool newMode);

		int* buffer;
		short maxVolume;

		Channel channels[9];		// OPLL chips have 9 channels
		byte instvol_r[9];		// instrument/volume (or volume/volume in percussive mode)

		unsigned eg_cnt;		// global envelope generator counter
		unsigned eg_timer;		// global envelope generator counter works at frequency = chipclock/72
		unsigned eg_timer_add;	    	// step of eg_timer
		unsigned eg_timer_overflow; 	// envelope generator timer overlfows every 1 sample (on real chip)

		bool rhythm;			// Rhythm mode

		// LFO
		unsigned lfo_am_cnt;
		unsigned lfo_am_inc;
		unsigned lfo_pm_cnt;
		unsigned lfo_pm_inc;

		int noise_rng;		// 23 bit noise shift register
		int noise_p;		// current noise 'phase'
		int noise_f;		// current noise period

		// instrument settings
		//   0     - user instrument
		//   1-15  - fixed instruments
		//   16    - bass drum settings
		//   17-18 - other percussion instruments
		byte inst_tab[19][8];

		int fn_tab[1024];		// fnumber->increment counter

		byte LFO_AM;
		byte LFO_PM;
};

#endif
