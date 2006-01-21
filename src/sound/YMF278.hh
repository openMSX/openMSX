// $Id$

// Based on ymf278b.c written by R. Belmont and O. Galibert

#ifndef YMF278_HH
#define YMF278_HH

#include "SoundDevice.hh"
#include "Clock.hh"
#include "openmsx.hh"
#include <memory>

namespace openmsx {

class Rom;
class MSXMotherBoard;
class DebugRegisters;
class DebugMemory;

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

	unsigned step;               // fixed-point frequency step
	unsigned stepptr;		// fixed-point pointer into the sample
	unsigned pos;
	short sample1, sample2;

	bool active;		// slot keyed on
	byte bits;		// width of the samples
	unsigned startaddr;
	unsigned loopaddr;
	unsigned endaddr;

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
	YMF278(MSXMotherBoard& motherBoard, const std::string& name,
	       int ramSize, const XMLElement& config, const EmuTime& time);
	virtual ~YMF278();
	void reset(const EmuTime& time);
	void writeRegOPL4(byte reg, byte data, const EmuTime& time);
	byte readReg(byte reg, const EmuTime& time);
	byte peekReg(byte reg) const;
	byte readStatus(const EmuTime& time);
	byte peekStatus(const EmuTime& time) const;

private:
	// SoundDevice
	virtual void setSampleRate(int sampleRate);
	virtual void setVolume(int newVolume);
	virtual void updateBuffer(unsigned length, int* buffer,
		const EmuTime& start, const EmuDuration& sampDur);

	void writeReg(byte reg, byte data, const EmuTime& time);
	byte readMem(unsigned address) const;
	void writeMem(unsigned address, byte value);
	short getSample(YMF278Slot& op);
	void advance();
	void checkMute();
	bool anyActive();
	void keyOnHelper(YMF278Slot& slot);

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

	YMF278Slot slots[24];

	/** Global envelope generator counter. */
	unsigned eg_cnt;
	/** Global envelope generator counter. */
	unsigned eg_timer;
	/** Step of eg_timer. */
	unsigned eg_timer_add;
	/** Envelope generator timer overlfows every 1 sample (on real chip). */
	unsigned eg_timer_overflow;

	char wavetblhdr;
	char memmode;
	int memadr;

	int fm_l, fm_r;
	int pcm_l, pcm_r;

	const std::auto_ptr<Rom> rom;
	byte* ram;
	unsigned endRom;
	unsigned endRam;

	/** Precalculated attenuation values with some marging for
	  * enveloppe and pan levels.
	  */
	int volume[256 * 4];

	double freqbase;

	byte regs[256];

	/** Time at which instrument loading is finished. */
	EmuTime loadTime;
	/** Time until which the YMF278 is busy. */
	EmuTime busyTime;

	friend class DebugRegisters;
	friend class DebugMemory;
	const std::auto_ptr<DebugRegisters> debugRegisters;
	const std::auto_ptr<DebugMemory>    debugMemory;
};

} // namespace openmsx

#endif
