// $Id$

#ifndef YM2413_2_HH
#define YM2413_2_HH

#include "YM2413Core.hh"
#include "SoundDevice.hh"
#include "Resample.hh"
#include "openmsx.hh"
#include <memory>

namespace openmsx {

class EmuTime;
class MSXMotherBoard;
class YM2413_2Debuggable;

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

class YM2413_2 : public YM2413Core, public SoundDevice, private Resample<1>
{
public:
	YM2413_2(MSXMotherBoard& motherBoard, const std::string& name,
	         const XMLElement& config, const EmuTime& time);
	virtual ~YM2413_2();

	void reset(const EmuTime& time);
	void writeReg(byte r, byte v, const EmuTime& time);

private:
	void checkMute();
	bool checkMuteHelper();

	void init_tables();

	inline void advance_lfo();
	inline void advance();
	inline void rhythm_calc(int** bufs, unsigned sample);
	inline int adjust(int x);

	inline void set_mul(byte slot, byte v);
	inline void set_ksl_tl(byte chan, byte v);
	inline void set_ksl_wave_fb(byte chan, byte v);
	inline void set_ar_dr(byte slot, byte v);
	inline void set_sl_rr(byte slot, byte v);
	void load_instrument(byte chan, byte slot, byte* inst);
	void update_instrument_zero(byte r);
	void setRhythmMode(bool newMode);

	// SoundDevice
	virtual void setVolume(int newVolume);
	virtual void setOutputRate(unsigned sampleRate);
	virtual void generateChannels(int** bufs, unsigned num);
	virtual void updateBuffer(unsigned length, int* buffer,
		const EmuTime& time, const EmuDuration& sampDur);

	// Resample
	virtual void generateInput(float* buffer, unsigned num);

	int maxVolume;

	Channel channels[9];	// OPLL chips have 9 channels
	byte instvol_r[9];	// instrument/volume (or volume/volume in percussive mode)

	unsigned eg_cnt;	// global envelope generator counter

	bool rhythm;		// Rhythm mode

	// LFO
	unsigned lfo_am_cnt;
	unsigned lfo_pm_cnt;

	int noise_rng;		// 23 bit noise shift register

	// instrument settings
	//   0     - user instrument
	//   1-15  - fixed instruments
	//   16    - bass drum settings
	//   17-18 - other percussion instruments
	byte inst_tab[19][8];

	byte LFO_AM;
	byte LFO_PM;

	byte reg[0x40];

	friend class YM2413_2Debuggable;
	const std::auto_ptr<YM2413_2Debuggable> debuggable;
};

} // namespace openmsx

#endif
