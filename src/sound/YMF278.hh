// $Id$

// Based on ymf278b.c written by R. Belmont and O. Galibert

#ifndef __YMF278__
#define __YMF278__

#include "openmsx.hh"
#include "SoundDevice.hh"
#include "Mixer.hh"
#include "Rom.hh"

namespace openmsx {

class MSXConfig;


class YMF278Slot
{
public:
	YMF278Slot();
	void reset();
	int compute_rate(int val);
	unsigned decay_rate(int num, int sample_rate);
	void envelope_next(int sample_rate);
	inline int compute_vib();
	inline int compute_am();
	void set_lfo(int newlfo);

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
	int  DL;
	char D2R;
	char RC;   		// rate correction
	char RR;

	int step;               // fixed-point frequency step
	int stepptr;		// fixed-point pointer into the sample
	int pos;
	short sample1, sample2;

	bool active;		// slot keyed on
	byte bits;		// width of the samples
	int startaddr;
	int loopaddr;
	int endaddr;

	byte state;
	int env_vol;
	unsigned env_vol_step;
	unsigned env_vol_lim;

	bool lfo_active;
	int lfo_cnt;
	int lfo_step;
	int lfo_max;
};

class YMF278 : public SoundDevice
{
public:
	YMF278(short volume, int ramSize, Device* config,
	       const EmuTime& time);
	virtual ~YMF278();
	void reset(const EmuTime& time);
	void writeRegOPL4(byte reg, byte data, const EmuTime& time);
	byte readRegOPL4(byte reg, const EmuTime& time);
	byte readStatus(const EmuTime& time);
	
	// SoundDevice
	virtual const string& getName() const;
	virtual const string& getDescription() const;
	virtual void setSampleRate(int sampleRate);
	virtual void setInternalVolume(short newVolume);
	virtual int* updateBuffer(int length);

private:
	byte readMem(unsigned address);
	void writeMem(unsigned address, byte value);
	short getSample(YMF278Slot& op);
	void advance();
	void checkMute();
	bool anyActive();
	void keyOnHelper(YMF278Slot& slot);

	static const int MASTER_CLK = 33868800;

	YMF278Slot slots[24];
	
	unsigned eg_cnt;	// global envelope generator counter
	unsigned eg_timer;	// global envelope generator counter
	unsigned eg_timer_add;		// step of eg_timer
	unsigned eg_timer_overflow;	// envelope generator timer overlfows every 1 sample (on real chip)
	
	char wavetblhdr;
	char memmode;
	int memadr;

	int fm_l, fm_r;
	int pcm_l, pcm_r;

	Rom rom;
	byte* ram;
	unsigned endRom;
	unsigned endRam;

	// precalculated attenuation values with some marging for
	// enveloppe and pan levels
	int volume[256 * 4];

	int* buffer;
	double freqbase;

	byte regs[256];

	EmuTimeFreq<MASTER_CLK> LD_Time;
	EmuTimeFreq<MASTER_CLK> BUSY_Time;
};

} // namespace openmsx

#endif
