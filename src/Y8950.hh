// $Id$

#ifndef _EMU8950_H_
#define _EMU8950_H_

//#include "ADPCM.hh"
#include "openmsx.hh"
#include "SoundDevice.hh"

//forward declarations
class EmuTime;


class Y8950 : public SoundDevice
{
	class Patch {
		public:
			Patch();
			void reset();
			int TL,FB,EG,ML,AR,DR,SL,RR,KR,KL,AM,PM;
	};
	class Slot {
		public:
			Slot();
			~Slot();
			void reset();

			static void makeSinTable();
			static void makeTllTable();
			static void makeAdjustTable();
			static void makeRksTable();
			static void makeDB2LinTable(short volume);
			static void makeDphaseARTable(int sampleRate);
			static void makeDphaseDRTable(int sampleRate);
			static void makeDphaseTable(int sampleRate);
			
			inline int calc_eg_dphase();
			inline void slotOn();
			inline void slotOff();

			inline int calc_phase(int lfo_pm);
			inline int calc_envelope(int lfo_am);
			inline int calc_slot_car(int lfo_pm, int lfo_am, int fm);
			inline int calc_slot_mod(int lfo_pm, int lfo_am);
			
			inline void UPDATE_ALL();
			inline void UPDATE_EG();
			inline void UPDATE_RKS();
			inline void UPDATE_TLL();
			inline void UPDATE_PG();


			// OUTPUT 
			int feedback;
			int output[5];	// Output value of slot 

			// for Phase Generator (PG) 
			int *sintbl;	// Wavetable 
			int phase;	// Phase 
			int dphase;	// Phase increment amount 
			int pgout;	// output 

			// for Envelope Generator (EG) 
			int fnum;	// F-Number 
			int block;	// Block 
			int tll;	// Total Level + Key scale level
			int rks;	// Key scale offset (Rks) 
			int eg_mode;	// Current state 
			int eg_phase;	// Phase 
			int eg_dphase;	// Phase increment amount 
			int egout;	// output 

			// LFO (refer to OPL->*) 
			int *plfo_am;
			int *plfo_pm;

			Patch patch;  

		private:
			static int lin2db(double d);
			inline static int ALIGN(int d, double SS, double SD);
			inline static int wave2_4pi(int e);
			inline static int wave2_8pi(int e);


			// Dynamic range of envelope 
			static const double EG_STEP = 0.1875;
			static const int EG_BITS = 9;
			static const int EG_MUTE = 1<<EG_BITS;
			// Dynamic range of sustine level 
			static const double SL_STEP = 3.0;
			static const int SL_BITS = 4;
			static const int SL_MUTE = 1<<SL_BITS;
			// Size of Sintable ( 1 -- 18 can be used, but 7 -- 14 recommended.)
			static const int PG_BITS = 10;
			static const int PG_WIDTH = 1<<PG_BITS;
			// Phase increment counter 
			static const int DP_BITS = 19;
			static const int DP_WIDTH = 1<<DP_BITS;
			static const int DP_BASE_BITS = DP_BITS - PG_BITS;
			// Bits for envelope phase incremental counter   
			static const int EG_DP_BITS = 23;
			static const int EG_DP_WIDTH = 1<<EG_DP_BITS;
			// Dynamic range of total level 
			static const double TL_STEP = 0.75;
			static const int TL_BITS = 6;
			static const int TL_MUTE = 1<<TL_BITS;
			// Bits for liner value 
			static const int DB2LIN_AMP_BITS = 11;
			static const int SLOT_AMP_BITS = DB2LIN_AMP_BITS;
			
			// WaveTable for each envelope amp 
			static int fullsintable[PG_WIDTH];
			// Phase incr table for Attack 
			static int dphaseARTable[16][16];
			// Phase incr table for Decay and Release 
			static int dphaseDRTable[16][16];
			// KSL + TL Table 
			static int tllTable[16][8][1<<TL_BITS][4];
			static int rksTable[2][8][2];
			// Phase incr table for PG 
			static int dphaseTable[1024][8][16]; 
			// Liner to Log curve conversion table (for Attack rate). 
			static int AR_ADJUST_TABLE[1<<EG_BITS];
	};

	class Channel {
		public:
			Channel();
			~Channel();
			void reset();

			bool alg;
			Slot mod, car;
	};

	public:
		Y8950(short volume);
		virtual ~Y8950();

		void reset();
		void writeReg(byte reg, byte data, const EmuTime &time);
		byte readReg(byte reg);
		byte readStatus();

		void setInternalVolume(short maxVolume);
		void setSampleRate(int sampleRate);
		int* updateBuffer(int length);


		// Cut the lower b bit(s) off. 
		inline static int HIGHBITS(int c, int b);
		// Leave the lower b bit(s). 
		inline static int LOWBITS(int c, int b);
		// Expand x which is s bits to d bits. 
		inline static int EXPAND_BITS(int x, int s, int d);
		// Expand x which is s bits to d bits and fill expanded bits '1' 
		inline static int EXPAND_BITS_X(int x, int s, int d);
		
		// Adjust envelope speed which depends on sampling rate. 
		inline static int rate_adjust(int x, int rate); 

	protected:
		// Definition of envelope mode 
		enum { SETTLE,ATTACK,DECAY,SUSHOLD,SUSTINE,RELEASE,FINISH };
		// Dynamic range 
		static const double DB_STEP = 0.1875;
		static const int DB_BITS = 9;
		static const int DB_MUTE = 1<<DB_BITS;
		// PM table is calcurated by PM_AMP * pow(2,PM_DEPTH*sin(x)/1200) 
		static const int PM_AMP_BITS = 8;
		static const int PM_AMP = 1<<PM_AMP_BITS;

	private:
		void makePmTable();
		void makeAmTable();

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
		inline void setFnumber(int c, int fnum);
		inline void setBlock(int c, int block);
		
		void update_noise();
		void update_ampm();
		
		inline int calcSample();
		void checkMute();
		bool checkMuteHelper();


		//ADPCM *adpcm;
		int adr;
		int output[2];
		// Register 
		unsigned char reg[0xff]; 
		int slot_on_flag[18];
		bool rythm_mode;
		// Pitch Modulator 
		int pm_mode;
		int pm_phase;
		// Amp Modulator 
		int am_mode;
		int am_phase;
		// Noise Generator 
		int noise_seed;
		// Channel & Slot 
		Channel ch[9];
		Slot *slot[18];
		int mask[10];	// mask[9] = RYTHM 

		static const int CLOCK_FREQ = 3579545;
		static const double PI = 3.14159265358979;
		// PM speed(Hz) and depth(cent) 
		static const double PM_SPEED = 6.4;
		static const double PM_DEPTH = (13.75/2);
		static const double PM_DEPTH2 = 13.75;
		// AM speed(Hz) and depth(dB) 
		static const double AM_SPEED = 3.7;
		static const double AM_DEPTH = 1.0;
		static const double AM_DEPTH2 = 4.8;

		static const int SLOT_BD1 = 12;
		static const int SLOT_BD2 = 13;
		static const int SLOT_HH  = 14;
		static const int SLOT_SD  = 15;
		static const int SLOT_TOM = 16;
		static const int SLOT_CYM = 17;
		// Bits for Pitch and Amp modulator 
		static const int PM_PG_BITS = 8;
		static const int PM_PG_WIDTH = 1<<PM_PG_BITS;
		static const int PM_DP_BITS = 16;
		static const int PM_DP_WIDTH = 1<<PM_DP_BITS;
		static const int AM_PG_BITS = 8;
		static const int AM_PG_WIDTH = 1<<AM_PG_BITS;
		static const int AM_DP_BITS = 16;
		static const int AM_DP_WIDTH = 1<<AM_DP_BITS;
		// LFO Table 
		int pmtable[2][PM_PG_WIDTH];
		int amtable[2][AM_PG_WIDTH];
		// dB to Liner table 
		static int dB2LinTab[(2*DB_MUTE)*2];

		int pm_dphase;
		int lfo_pm;
		int am_dphase;
		int lfo_am;
		int whitenoise;

		int* buffer;
};

#endif
