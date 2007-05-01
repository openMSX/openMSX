// $Id$
/*
 **
 ** File: ym2151.h - header file for software implementation of YM2151
 **                                            FM Operator Type-M(OPM)
 **
 ** (c) 1997-2002 Jarek Burczynski (s0246@poczta.onet.pl, bujar@mame.net)
 ** Some of the optimizing ideas by Tatsuyuki Satoh
 **
 ** Version 2.150 final beta May, 11th 2002
 **
 **
 ** I would like to thank following people for making this project possible:
 **
 ** Beauty Planets - for making a lot of real YM2151 samples and providing
 ** additional informations about the chip. Also for the time spent making
 ** the samples and the speed of replying to my endless requests.
 **
 ** Shigeharu Isoda - for general help, for taking time to scan his YM2151
 ** Japanese Manual first of all, and answering MANY of my questions.
 **
 ** Nao - for giving me some info about YM2151 and pointing me to Shigeharu.
 ** Also for creating fmemu (which I still use to test the emulator).
 **
 ** Aaron Giles and Chris Hardy - they made some samples of one of my favourite
 ** arcade games so I could compare it to my emulator.
 **
 ** Bryan McPhail and Tim (powerjaw) - for making some samples.
 **
 ** Ishmair - for the datasheet and motivation.
 */

#ifndef YM2151_HH
#define YM2151_HH

#include "SoundDevice.hh"
#include "EmuTimer.hh"
#include "Resample.hh"
#include "IRQHelper.hh"
#include "openmsx.hh"
#include <memory>

namespace openmsx {

class MSXMotherBoard;

class YM2151 : public SoundDevice, private EmuTimerCallback, private Resample<2>
{
public:
	YM2151(MSXMotherBoard& motherBoard, const std::string& name,
	       const std::string& desc, const XMLElement& config,
	       const EmuTime& /*time*/);
	~YM2151();
	void reset(const EmuTime& time);
	void writeReg(byte r, byte v, const EmuTime& time);
	int readStatus();

private:
	// a single operator
	struct YM2151Operator {
		unsigned phase;    // accumulated operator phase
		unsigned freq;     // operator frequency count
		int dt1;           // current DT1 (detune 1 phase inc/decrement) value
		unsigned mul;      // frequency count multiply
		unsigned dt1_i;    // DT1 index * 32
		unsigned dt2;      // current DT2 (detune 2) value

		int mem_value;     // delayed sample (MEM) value

		// channel specific data
		// note: each operator number 0 contains channel specific data
		unsigned fb_shift; // feedback shift value for operators 0 in each channel
		int fb_out_curr;   // operator feedback value (used only by operators 0)
		int fb_out_prev;   // previous feedback value (used only by operators 0)
		unsigned kc;       // channel KC (copied to all operators)
		unsigned kc_i;     // just for speedup
		unsigned pms;      // channel PMS
		unsigned ams;      // channel AMS

		unsigned AMmask;   // LFO Amplitude Modulation enable mask
		unsigned state;    // Envelope state: 4-attack(AR)
		                   //                 3-decay(D1R)
		                   //                 2-sustain(D2R)
		                   //                 1-release(RR)
		                   //                 0-off
		unsigned tl;       // Total attenuation Level
		int volume;        // current envelope attenuation level
		unsigned d1l;      // envelope switches to sustain state after

		unsigned key;      // 0=last key was KEY OFF, 1=last key was KEY ON

		unsigned ks;       // key scale
		unsigned ar;       // attack rate
		unsigned d1r;      // decay rate
		unsigned d2r;      // sustain rate
		unsigned rr;       // release rate

		int* connect;      // operator output 'direction'
		int* mem_connect;  // where to put the delayed sample (MEM)

		byte eg_sh_ar;     //  (attack state)
		byte eg_sel_ar;    //  (attack state)
		byte eg_sh_d1r;    //  (decay state)
		byte eg_sel_d1r;   //  (decay state)
		                   // reaching this level
		byte eg_sh_d2r;    //  (sustain state)
		byte eg_sel_d2r;   //  (sustain state)
		byte eg_sh_rr;     //  (release state)
		byte eg_sel_rr;    //  (release state)
	};

	void setConnect(YM2151Operator* om1, int cha, int v);

	// SoundDevice
	virtual void setVolume(int maxVolume);
	virtual void setOutputRate(unsigned sampleRate);
	virtual void generateChannels(int** bufs, unsigned num);
	virtual bool updateBuffer(unsigned length, int* buffer,
	                          const EmuTime& start, const EmuDuration& sampDur);

	 // Resample
        virtual bool generateInput(float* buffer, unsigned num);

	void callback(byte flag);
	void setStatus(byte flags);
	void resetStatus(byte flags);

	void initTables();
	void initChipTables();

	// operator methods
	void envelopeKONKOFF(YM2151Operator* op, int v);
	static void refreshEG(YM2151Operator* op);
	int opCalc(YM2151Operator* op, unsigned env, int pm);
	int opCalc1(YM2151Operator* op, unsigned env, int pm);
	inline unsigned volumeCalc(YM2151Operator* op, unsigned AM);
	inline void keyOn(YM2151Operator* op, unsigned keySet);
	inline void keyOff(YM2151Operator* op, unsigned keyClear);

	// general chip mehods
	void chanCalc(unsigned chan);
	void chan7Calc();

	void advanceEG();
	void advance();

	bool checkMuteHelper();
	int adjust(int x);

	IRQHelper irq;

	// Timers (see EmuTimer class for details about timing)
	EmuTimerOPM_1 timer1;
	EmuTimerOPM_2 timer2;

	YM2151Operator oper[32]; // the 32 operators

	unsigned pan[16];        // channels output masks (0xffffffff = enable)

	unsigned eg_cnt;         // global envelope generator counter
	unsigned eg_timer;       // global envelope generator counter
	                         //   works at frequency = chipclock/64/3
	unsigned lfo_phase;      // accumulated LFO phase (0 to 255)
	unsigned lfo_timer;      // LFO timer
	unsigned lfo_overflow;   // LFO generates new output when lfo_timer
	                         // reaches this value
	unsigned lfo_counter;    // LFO phase increment counter
	unsigned lfo_counter_add;// step of lfo_counter
	unsigned lfa;            // LFO current AM output
	int lfp;                 // LFO current PM output

	unsigned noise;          // noise enable/period register
	                         // bit 7 - noise enable, bits 4-0 - noise period
	unsigned noise_rng;      // 17 bit noise shift register
	int noise_p;             // current noise 'phase'
	unsigned noise_f;        // current noise period

	unsigned csm_req;        // CSM  KEY ON / KEY OFF sequence request

	unsigned irq_enable;     // IRQ enable for timer B (bit 3) and timer A
	                         // (bit 2); bit 7 - CSM mode (keyon to all 
	                         // slots, everytime timer A overflows)
	unsigned status;         // chip status (BUSY, IRQ Flags)

	// Frequency-deltas to get the closest frequency possible.
	// There are 11 octaves because of DT2 (max 950 cents over base frequency)
	// and LFO phase modulation (max 800 cents below AND over base frequency)
	// Summary:   octave  explanation
	//             0       note code - LFO PM
	//             1       note code
	//             2       note code
	//             3       note code
	//             4       note code
	//             5       note code
	//             6       note code
	//             7       note code
	//             8       note code
	//             9       note code + DT2 + LFO PM
	//            10       note code + DT2 + LFO PM
	unsigned freq[11 * 768]; // 11 octaves, 768 'cents' per octave   // No Save

	// Frequency deltas for DT1. These deltas alter operator frequency
	// after it has been taken from frequency-deltas table.
	int dt1_freq[8 * 32];    // 8 DT1 levels, 32 KC values         // No Save
	unsigned noise_tab[32];  // 17bit Noise Generator periods      // No Save

	int chanout[8];
	int m2, c1, c2;          // Phase Modulation input for operators 2,3,4
	int mem;                 // one sample delay memory

	int maxVolume;

	word timer_A_val;

	byte lfo_wsel;           // LFO waveform (0-saw, 1-square, 2-triangle,
	                         //               3-random noise)
	byte amd;                // LFO Amplitude Modulation Depth
	signed char pmd;         // LFO Phase Modulation Depth

	byte test;               // TEST register
	byte ct;                 // output control pins (bit1-CT2, bit0-CT1)

};

} // namespace openmsx

#endif /*YM2151_HH*/
