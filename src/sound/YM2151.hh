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

// YM2164/OPP specifics info from http://map.grauw.nl/resources/sound/yamaha_ym2164.php

#ifndef YM2151_HH
#define YM2151_HH

#include "ResampledSoundDevice.hh"
#include "EmuTimer.hh"
#include "EmuTime.hh"
#include "IRQHelper.hh"
#include "openmsx.hh"
#include <array>
#include <string>
#include <memory>

namespace openmsx {

class DeviceConfig;

class YM2151 final : public ResampledSoundDevice, private EmuTimerCallback
{
public:
	enum class Variant : char {
	    YM2151,   // aka OPM
	    YM2164,   // aka OPP
	};

	YM2151(const std::string& name, static_string_view desc,
	       const DeviceConfig& config, EmuTime::param time, Variant variant);
	~YM2151();

	void reset(EmuTime::param time);
	void writeReg(byte r, byte v, EmuTime::param time);
	[[nodiscard]] byte readStatus() const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// a single operator
	struct YM2151Operator {
		template<typename Archive>
		void serialize(Archive& ar, unsigned version);

		int* connect;      // operator output 'direction'
		int* mem_connect;  // where to put the delayed sample (MEM)

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

	void setConnect(std::span<YM2151Operator, 4> o, int cha, int v);

	// SoundDevice
	void generateChannels(std::span<float*> bufs, unsigned num) override;

	void callback(byte flag) override;
	void setStatus(byte flags);
	void resetStatus(byte flags);

	// operator methods
	void envelopeKONKOFF(std::span<YM2151Operator, 4> op, int v);
	static void refreshEG(std::span<YM2151Operator, 4> op);
	[[nodiscard]] int opCalc(YM2151Operator& op, unsigned env, int pm);
	[[nodiscard]] int opCalc1(YM2151Operator& op, unsigned env, int pm);
	[[nodiscard]] inline unsigned volumeCalc(YM2151Operator& op, unsigned AM);
	inline void keyOn(YM2151Operator& op, unsigned keySet);
	inline void keyOff(YM2151Operator& op, unsigned keyClear);

	// general chip mehods
	void chanCalc(unsigned chan);
	void chan7Calc();

	void advanceEG();
	void advance();

	[[nodiscard]] bool checkMuteHelper();

	IRQHelper irq;

	// Timers (see EmuTimer class for details about timing)
	const std::unique_ptr<EmuTimer> timer1;
	const std::unique_ptr<EmuTimer> timer2;

	std::array<YM2151Operator, 32> oper; // the 32 operators

	std::array<unsigned, 16> pan; // channels output masks (0xffffffff = enable)

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

	std::array<int, 8> chanOut;
	int m2, c1, c2;          // Phase Modulation input for operators 2,3,4
	int mem;                 // one sample delay memory

	word timer_A_val;

	byte lfo_wsel;           // LFO waveform (0-saw, 1-square, 2-triangle,
	                         //               3-random noise)
	byte amd;                // LFO Amplitude Modulation Depth
	signed char pmd;         // LFO Phase Modulation Depth

	byte test;               // TEST register
	byte ct;                 // output control pins (bit1-CT2, bit0-CT1)

	std::array<byte, 256> regs; // only used for serialization ATM
	const Variant variant;   // Whether we're emulating YM2151 or YM2164
};

} // namespace openmsx

#endif
