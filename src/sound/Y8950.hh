// $Id$

#ifndef __Y8950_HH__
#define __Y8950_HH__

#include "openmsx.hh"
#include "SoundDevice.hh"
#include "IRQHelper.hh"
#include "IRQHelper.ii"
#include "Mixer.hh"
#include "Y8950Timer.hh"
#include "Y8950Adpcm.hh"
#include "Y8950KeyboardConnector.hh"


class Y8950 : public SoundDevice
{
	class Patch {
		public:
			Patch();
			void reset();

			bool AM, PM, EG;
			byte KR; // 0-1
			byte ML; // 0-15
			byte KL; // 0-3
			byte TL; // 0-63
			byte FB; // 0-7
			byte AR; // 0-15
			byte DR; // 0-15
			byte SL; // 0-15
			byte RR; // 0-15
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
			static void makeDB2LinTable();
			static void makeDphaseARTable(int sampleRate);
			static void makeDphaseDRTable(int sampleRate);
			static void makeDphaseTable(int sampleRate);
			
			inline void slotOn();
			inline void slotOff();

			inline void calc_phase();
			inline void calc_envelope();
			inline int calc_slot_car(int fm);
			inline int calc_slot_mod();
			inline int calc_slot_tom();
			inline int calc_slot_snare(int whitenoise);
			inline int calc_slot_cym(int a, int b);
			inline int calc_slot_hat(int a, int b, int whitenoise);
			
			inline void updateAll();
			inline void updateEG();
			inline void updateRKS();
			inline void updateTLL();
			inline void updatePG();


			// OUTPUT 
			int feedback;
			int output[5];	// Output value of slot 

			// for Phase Generator (PG) 
			unsigned int phase;	// Phase 
			unsigned int dphase;	// Phase increment amount 
			int pgout;		// output 

			// for Envelope Generator (EG) 
			int fnum;		// F-Number 
			int block;		// Block 
			int tll;		// Total Level + Key scale level
			int rks;		// Key scale offset (Rks) 
			int eg_mode;		// Current state 
			unsigned int eg_phase;	// Phase 
			unsigned int eg_dphase;	// Phase increment amount 
			int egout;		// output 

			bool slotStatus;
			Patch patch;  
			
			// refer to Y8950->
			int *plfo_pm;
			int *plfo_am;

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
			
			// WaveTable for each envelope amp 
			static int sintable[PG_WIDTH];
			// Phase incr table for Attack 
			static unsigned int dphaseARTable[16][16];
			// Phase incr table for Decay and Release 
			static unsigned int dphaseDRTable[16][16];
			// KSL + TL Table 
			static int tllTable[16][8][1<<TL_BITS][4];
			static int rksTable[2][8][2];
			// Phase incr table for PG 
			static unsigned int dphaseTable[1024][8][16]; 
			// Liner to Log curve conversion table (for Attack rate). 
			static int AR_ADJUST_TABLE[1<<EG_BITS];
	};
	friend class Slot;

	class Channel {
		public:
			Channel();
			~Channel();
			void reset();
			inline void setFnumber(int fnum);
			inline void setBlock(int block);
			inline void keyOn();
			inline void keyOff();

			bool alg;
			Slot mod, car;
	};

	public:
		Y8950(short volume, int sampleRam, const EmuTime &time,
		      Mixer::ChannelMode mode=Mixer::MONO);
		virtual ~Y8950();

		void reset(const EmuTime &time);
		void writeReg(byte reg, byte data, const EmuTime &time);
		byte readReg(byte reg, const EmuTime &time);
		byte readStatus();

		virtual void setInternalVolume(short maxVolume);
		virtual void setSampleRate(int sampleRate);
		virtual int* updateBuffer(int length);

	private:
		// Definition of envelope mode 
		enum { ATTACK,DECAY,SUSHOLD,SUSTINE,RELEASE,FINISH };
		// Dynamic range 
		static const double DB_STEP = 0.1875;
		static const int DB_BITS = 9;
		static const int DB_MUTE = 1<<DB_BITS;
		// PM table is calcurated by PM_AMP * pow(2,PM_DEPTH*sin(x)/1200) 
		static const int PM_AMP_BITS = 8;
		static const int PM_AMP = 1<<PM_AMP_BITS;
		
		static unsigned int dphaseNoiseTable[1024][8];
		
		inline static int DB_POS(int x);
		inline static int DB_NEG(int x);
		inline static int HIGHBITS(int c, int b);
		inline static int LOWBITS(int c, int b);
		inline static int EXPAND_BITS(int x, int s, int d);
		static unsigned int rate_adjust(double x, int rate); 
		
		void makeDphaseNoiseTable(int sampleRate);
		void makePmTable();
		void makeAmTable();

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
		
		inline int calcSample(int channelMask);
		void checkMute();
		bool checkMuteHelper();

		void setStatus(int flags);
		void resetStatus(int flags);
		void changeStatusMask(int newMask);

		int adr;
		int output[2];
		// Register 
		byte reg[0xff]; 
		bool rythm_mode;
		// Pitch Modulator 
		int pm_mode;
		unsigned int pm_phase;
		// Amp Modulator 
		int am_mode;
		unsigned int am_phase;
		
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
		// Bits for liner value 
		static const int DB2LIN_AMP_BITS = 11;
		static const int SLOT_AMP_BITS = DB2LIN_AMP_BITS;

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
		static short dB2LinTab[(2*DB_MUTE)*2];

		unsigned int pm_dphase;
		int lfo_pm;
		unsigned int am_dphase;
		int lfo_am;

		int* buffer;
		short maxVolume;


		// Bitmask for register 0x04 
		static const int R04_ST1          = 0x01;	// Timer1 Start
		static const int R04_ST2          = 0x02;	// Timer2 Start
		//static const int R04            = 0x04;	// not used
		static const int R04_MASK_BUF_RDY = 0x08;	// Mask 'Buffer Ready'
		static const int R04_MASK_EOS     = 0x10;	// Mask 'End of sequence' 
		static const int R04_MASK_T2      = 0x20;	// Mask Timer2 flag 
		static const int R04_MASK_T1      = 0x40;	// Mask Timer1 flag 
		static const int R04_IRQ_RESET    = 0x80;	// IRQ RESET 

		// Bitmask for status register 
		static const int STATUS_EOS     = R04_MASK_EOS;
		static const int STATUS_BUF_RDY = R04_MASK_BUF_RDY;
		static const int STATUS_T2      = R04_MASK_T2;
		static const int STATUS_T1      = R04_MASK_T1;

		byte status;		// STATUS Register
		byte statusMask;	//  bit=0 -> masked 
		IRQHelper irq;
		
		// Timers
		Y8950Timer<12500, STATUS_T1> timer1;	//  80us
		friend class Y8950Timer<12500, STATUS_T1>;
		Y8950Timer< 3125, STATUS_T2> timer2;	// 320us
		friend class Y8950Timer< 3125, STATUS_T2>;
		
		// ADPCM
		Y8950Adpcm adpcm;
		friend class Y8950Adpcm;

		// Keyboard connector
		Y8950KeyboardConnector connector;
};

#endif
