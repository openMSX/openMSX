#ifndef __YM2413_HH___
#define __YM2413_HH__

#include "openmsx.hh"
#include "Mixer.hh"
#include "SoundDevice.hh"


class YM2413 : public SoundDevice
{
	class Patch {
		public:
			Patch();
			int TL,FB,EG,ML,AR,DR,SL,RR,KR,KL,AM,PM,WF;
	};
	class Slot {
		public:
			Slot(int type, short* volTb);
			~Slot();
			void reset();

			inline int calc_eg_dphase();
			inline void slotOn();
			inline void slotOff();
			inline void setSlotPatch(Patch *patch);
			inline void setSlotVolume(int volume);
			inline int calc_phase();
			inline int calc_envelope();
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
		
			// volume table
			short* volTab;
			
			Patch *patch;  
			int type;		// 0 : modulator 1 : carrier 

			// OUTPUT
			int feedback;
			int output[5];	// Output value of slot 

			// for Phase Generator (PG)
			word *sintbl;		// Wavetable 
			int phase;		// Phase 
			int dphase;		// Phase increment amount 
			int pgout;		// output 

			// for Envelope Generator (EG) 
			int fnum;		// F-Number 
			int block;		// Block 
			int volume;		// Current volume 
			int sustine;		// Sustine 1 = ON, 0 = OFF 
			int tll;		// Total Level + Key scale level
			int rks;		// Key scale offset (Rks) 
			int eg_mode;		// Current state 
			int eg_phase;		// Phase 
			int eg_dphase;		// Phase increment amount 
			int egout;		// output 

			// refer to opll-> 
			int *plfo_pm;
			int *plfo_am;
	};
	class Channel {
		public:
			Channel(short* volTab);
			~Channel();
			void reset();
		
			int patch_number;
			bool key_status;
			Slot *mod, *car;
	};
	
	public:
		YM2413();
		virtual ~YM2413();

		void reset();
		void writeReg(byte reg, byte value);
		
		void setInternalVolume(short maxVolume);
		void setSampleRate(int sampleRate);
		int* updateBuffer(int length);

	private:
		void init();
		int calcSample();
	
		void checkMute();
		bool checkMuteHelper();
	
		static void makeAdjustTable();
		static void makeSinTable();
		static int lin2db(double d);
		inline static int min(int i, int j);
		static void makeDphaseNoiseTable(int sampleRate);
		static void makePmTable();
		static void makeAmTable();
		static void makeDphaseTable(int sampleRate);
		static void makeTllTable();
		static void makeDphaseARTable(int sampleRate);
		static void makeDphaseDRTable(int sampleRate);
		static void makeRksTable();
		static void makeDefaultPatch();
		static void getDefaultPatch(int num, Patch *patch);
		static void dump2patch(const byte *dump, Patch *patch);
		static byte default_inst[(16+3)*16];
		void reset_patch();
		void copyPatch(int num, Patch *patch);
		inline void keyOn(int i);
		inline void keyOff(int i);
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
		inline void setPatch(int i, int num);
		inline void setSustine(int c, int sustine);
		inline void setVol(int c, int volume);
		inline void setFnumber(int c, int fnum);
		inline void setBlock(int c, int block);
		inline void setRythmMode(int data);
		inline void update_noise();
		inline void update_ampm();
	
	public:
		inline static int TL2EG(int d);
		inline static int DB_POS(int x);
		inline static int DB_NEG(int x);
		inline static int HIGHBITS(int c, int b);
		inline static int LOWBITS(int c, int b);
		inline static int EXPAND_BITS(int x, int s, int d);
		inline static int rate_adjust(int x, int sampleRate);

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
		static const int DB2LIN_AMP_BITS = 15; /////
		static const int SLOT_AMP_BITS = (DB2LIN_AMP_BITS);

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

		static const int SLOT_BD1 = 12;
		static const int SLOT_BD2 = 13;
		static const int SLOT_HH = 14;
		static const int SLOT_SD = 15;
		static const int SLOT_TOM = 16;
		static const int SLOT_CYM = 17;
		
		int* buffer;

		int output[2];

		// Register 
		byte reg[0x40]; 
		byte slot_on_flag[18];

		// Rythm Mode 
		bool rythm_mode;

		// Pitch Modulator 
		int pm_phase;
		int lfo_pm;

		// Amp Modulator 
		int am_phase;
		int lfo_am;

		// Noise Generator 
		int noise_seed;
		int whitenoise;
		int noiseA;
		int noiseB;
		int noiseA_phase;
		int noiseB_phase;
		int noiseA_dphase;
		int noiseB_dphase;

		// Channel & Slot 
		Channel *ch[9];
		Slot *slot[18];

		// Voice Data 
		Patch* patch[19*2];
		int patch_update[2]; // flag for check patch update 

		// dB to linear table (used by Slot)
		short dB2LinTab[(DB_MUTE+DB_MUTE)*2];


		//// static variables ////
		// These are share between all YM2413 objects
		// They only need to be calculated once
		static bool alreadyInitialized;

		// WaveTable for each envelope amp 
		static word fullsintable[PG_WIDTH];
		static word halfsintable[PG_WIDTH];

		static int dphaseNoiseTable[512][8];

		static word *waveform[2];

		// LFO Table 
		static int pmtable[PM_PG_WIDTH];
		static int amtable[AM_PG_WIDTH];

		// Noise and LFO 
		static int pm_dphase;
		static int am_dphase;

		// Liner to Log curve conversion table (for Attack rate). 
		static word AR_ADJUST_TABLE[1<<EG_BITS];

		// Empty voice data 
		static Patch null_patch;

		// Basic voice Data 
		static Patch default_patch[(16+3)*2];

		// Definition of envelope mode 
		enum { SETTLE,ATTACK,DECAY,SUSHOLD,SUSTINE,RELEASE,FINISH };

		// Phase incr table for Attack 
		static int dphaseARTable[16][16];
		// Phase incr table for Decay and Release 
		static int dphaseDRTable[16][16];

		// KSL + TL Table 
		static int tllTable[16][8][1<<TL_BITS][4];
		static int rksTable[2][8][2];

		// Phase incr table for PG 
		static int dphaseTable[512][8][16];
};

#endif
