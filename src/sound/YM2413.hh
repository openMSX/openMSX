// $Id$

#ifndef __YM2413_HH__
#define __YM2413_HH__

#include "openmsx.hh"
#include "SoundDevice.hh"
#include "Mixer.hh"
#include "YM2413Core.hh"
#include "Debuggable.hh"

namespace openmsx {

class EmuTime;

class YM2413 : public YM2413Core, private SoundDevice, private Debuggable
{
	class Patch {
	public:
		Patch() {}
		Patch(bool AM, bool PM, bool EG, byte KR, byte ML,
		      byte KL, byte TL, byte FB, byte WF, byte AR,
		      byte DR, byte SL, byte RR);
		
		bool AM, PM, EG;
		byte KR; // 0-1
		byte ML; // 0-15
		byte KL; // 0-3
		byte TL; // 0-63
		byte FB; // 0-7
		byte WF; // 0-1
		byte AR; // 0-15
		byte DR; // 0-15
		byte SL; // 0-15
		byte RR; // 0-15
	};
	
	class Slot {
	public:
		Slot(bool type);
		~Slot();
		void reset();

		inline void slotOn();
		inline void slotOff();
		inline void setPatch(Patch* patch);
		inline void setVolume(int volume);
		inline void calc_phase();
		inline void calc_envelope();
		inline int calc_slot_car(int fm);
		inline int calc_slot_mod();
		inline int calc_slot_tom();
		inline int calc_slot_snare(int whitenoise);
		inline int calc_slot_cym(int a, int b);
		inline int calc_slot_hat(int a, int b, int whitenoise);
		inline void updatePG();
		inline void updateTLL();
		inline void updateRKS();
		inline void updateWF();
		inline void updateEG();
		inline void updateAll();
		inline static int wave2_4pi(int e);
		inline static int wave2_8pi(int e);
		inline static int EG2DB(int d);
		inline static int SL2EG(int d);
	
		Patch* patch;  
		bool type;		// 0 : modulator 1 : carrier 
		bool slotStatus;

		// OUTPUT
		int feedback;
		int output[5];	// Output value of slot 

		// for Phase Generator (PG)
		word* sintbl;		// Wavetable
		unsigned int phase;	// Phase 
		unsigned int dphase;	// Phase increment amount 
		int pgout;		// output

		// for Envelope Generator (EG)
		int fnum;		// F-Number
		int block;		// Block
		int volume;		// Current volume
		int sustine;		// Sustine 1 = ON, 0 = OFF
		int tll;		// Total Level + Key scale level
		int rks;		// Key scale offset (Rks)
		int eg_mode;		// Current state
		unsigned int eg_phase;	// Phase
		unsigned int eg_dphase;	// Phase increment amount
		int egout;		// output

		// refer to YM2413->
		int* plfo_pm;
		int* plfo_am;
	};
	friend class Slot;
	
	class Channel {
	public:
		Channel();
		~Channel();
		void reset();
		inline void setPatch(int num);
		inline void setSustine(bool sustine);
		inline void setVol(int volume);
		inline void setFnumber(int fnum);
		inline void setBlock(int block);
		inline void keyOn();
		inline void keyOff();

		Patch* patches;
		bool userPatch;
		Slot mod, car;
	};

public:
	YM2413(const string& name, short volume, const EmuTime &time,
	       Mixer::ChannelMode mode = Mixer::MONO);
	virtual ~YM2413();

	void reset(const EmuTime &time);
	void writeReg(byte reg, byte value, const EmuTime &time);

	// SoundDevice
	virtual const string& getName() const;
	virtual const string& getDescription() const;
	virtual void setVolume(int maxVolume);
	virtual void setSampleRate(int sampleRate);
	virtual int* updateBuffer(int length) throw();

private:
	inline int calcSample(int channelMask);

	void checkMute();
	bool checkMuteHelper();

	static void makeAdjustTable();
	static void makeSinTable();
	static int lin2db(double d);
	static void makeDphaseNoiseTable(int sampleRate);
	static void makePmTable();
	static void makeAmTable();
	static void makeDphaseTable(int sampleRate);
	static void makeTllTable();
	static void makeDphaseARTable(int sampleRate);
	static void makeDphaseDRTable(int sampleRate);
	static void makeRksTable();
	static void makeDB2LinTable();
	
	inline void keyOn_BD();
	inline void keyOn_SD();
	inline void keyOn_TOM();
	inline void keyOn_HH();
	inline void keyOn_CYM();
	inline void keyOff_BD();
	inline void keyOff_SD();
	inline void keyOff_TOM();
	inline void keyOff_HH();
	inline void keyOff_CYM();
	inline void setRythmMode(int data);
	inline void update_noise();
	inline void update_ampm();

	inline static int TL2EG(int d);
	inline static int DB_POS(int x);
	inline static int DB_NEG(int x);
	inline static int HIGHBITS(int c, int b);
	inline static int LOWBITS(int c, int b);
	inline static int EXPAND_BITS(int x, int s, int d);
	inline static unsigned int rate_adjust(double x, int sampleRate);

	// Debuggable
	virtual unsigned getSize() const;
	//virtual const string& getDescription() const;  // also in SoundDevice!!
	virtual byte read(unsigned address);
	virtual void write(unsigned address, byte value);

private:
	static const int CLOCK_FREQ = 3579545;
	static const double PI = 3.14159265358979;

	// Size of Sintable ( 1 -- 18 can be used, but 7 -- 14 recommended.)
	static const int PG_BITS = 9;
	static const int PG_WIDTH = (1<<PG_BITS);

	// Phase increment counter
	static const int DP_BITS = 18;
	static const int DP_WIDTH = (1<<DP_BITS);
	static const int DP_BASE_BITS = (DP_BITS - PG_BITS);

	// Dynamic range (Accuracy of sin table)
	static const int DB_BITS = 8;
	static const double DB_STEP = (48.0/(1<<DB_BITS));
	static const int DB_MUTE = (1<<DB_BITS);

	// Dynamic range of envelope
	static const double EG_STEP = 0.375;
	static const int EG_BITS = 7;
	static const int EG_MUTE = (1<<EG_BITS);

	// Dynamic range of total level
	static const double TL_STEP = 0.75;
	static const int TL_BITS = 6;
	static const int TL_MUTE = (1<<TL_BITS);

	// Dynamic range of sustine level
	static const double SL_STEP = 3.0;
	static const int SL_BITS = 4;
	static const int SL_MUTE = (1<<SL_BITS);

	// Bits for liner value
	static const int DB2LIN_AMP_BITS = 11;
	static const int SLOT_AMP_BITS = DB2LIN_AMP_BITS;

	// Bits for envelope phase incremental counter
	static const int EG_DP_BITS = 22;
	static const int EG_DP_WIDTH = (1<<EG_DP_BITS);

	// Bits for Pitch and Amp modulator 
	static const int PM_PG_BITS = 8;
	static const int PM_PG_WIDTH = (1<<PM_PG_BITS);
	static const int PM_DP_BITS = 16;
	static const int PM_DP_WIDTH = (1<<PM_DP_BITS);
	static const int AM_PG_BITS = 8;
	static const int AM_PG_WIDTH = (1<<AM_PG_BITS);
	static const int AM_DP_BITS = 16;
	static const int AM_DP_WIDTH = (1<<AM_DP_BITS);

	// PM table is calcurated by PM_AMP * pow(2,PM_DEPTH*sin(x)/1200) 
	static const int PM_AMP_BITS = 8;
	static const int PM_AMP = (1<<PM_AMP_BITS);

	// PM speed(Hz) and depth(cent) 
	static const double PM_SPEED = 6.4;
	static const double PM_DEPTH = 13.75;

	// AM speed(Hz) and depth(dB)
	static const double AM_SPEED = 3.7;
	static const double AM_DEPTH = 2.4;

	int* buffer;
	int maxVolume;

	int output[2];

	// Register
	byte reg[0x40];

	// Rythm Mode
	bool rythm_mode;

	// Pitch Modulator
	unsigned int pm_phase;
	int lfo_pm;

	// Amp Modulator
	unsigned int am_phase;
	int lfo_am;

	// Noise Generator
	int noise_seed;
	int whitenoise;
	int noiseA;
	int noiseB;
	unsigned int noiseA_phase;
	unsigned int noiseB_phase;
	unsigned int noiseA_dphase;
	unsigned int noiseB_dphase;

	// Channel & Slot
	Channel ch[9];
	Slot *slot[18];

	// Empty voice data
	static Patch nullPatch;

	// Voice Data
	Patch patches[19*2];

	// dB to linear table (used by Slot)
	static short dB2LinTab[(DB_MUTE+DB_MUTE)*2];

	// WaveTable for each envelope amp
	static word fullsintable[PG_WIDTH];
	static word halfsintable[PG_WIDTH];

	static unsigned int dphaseNoiseTable[512][8];

	static word *waveform[2];

	// LFO Table
	static int pmtable[PM_PG_WIDTH];
	static int amtable[AM_PG_WIDTH];

	// Noise and LFO
	static unsigned int pm_dphase;
	static unsigned int am_dphase;

	// Liner to Log curve conversion table (for Attack rate).
	static word AR_ADJUST_TABLE[1<<EG_BITS];

	// Definition of envelope mode
	enum { ATTACK,DECAY,SUSHOLD,SUSTINE,RELEASE,FINISH };

	// Phase incr table for Attack
	static unsigned int dphaseARTable[16][16];
	// Phase incr table for Decay and Release
	static unsigned int dphaseDRTable[16][16];

	// KSL + TL Table
	static int tllTable[16][8][1<<TL_BITS][4];
	static int rksTable[2][8][2];

	// Phase incr table for PG 
	static unsigned int dphaseTable[512][8][16];

	const string name;
};

} // namespace openmsx

#endif
