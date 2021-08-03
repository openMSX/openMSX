#ifndef YMF262_HH
#define YMF262_HH

#include "ResampledSoundDevice.hh"
#include "SimpleDebuggable.hh"
#include "EmuTimer.hh"
#include "EmuTime.hh"
#include "FixedPoint.hh"
#include "IRQHelper.hh"
#include "openmsx.hh"
#include "serialize_meta.hh"
#include <string>

namespace openmsx {

class DeviceConfig;

class YMF262 final : private ResampledSoundDevice, private EmuTimerCallback
{
public:
	YMF262(const std::string& name, const DeviceConfig& config,
	       bool isYMF278);
	~YMF262();

	void reset(EmuTime::param time);
	void writeReg   (unsigned r, byte v, EmuTime::param time);
	void writeReg512(unsigned r, byte v, EmuTime::param time);
	[[nodiscard]] byte readReg(unsigned reg);
	[[nodiscard]] byte peekReg(unsigned reg) const;
	[[nodiscard]] byte readStatus();
	[[nodiscard]] byte peekStatus() const;

	void setMixLevel(uint8_t x, EmuTime::param time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

public:
	/** 16.16 fixed point type for frequency calculations */
	using FreqIndex = FixedPoint<16>;

	enum EnvelopeState {
		EG_ATTACK, EG_DECAY, EG_SUSTAIN, EG_RELEASE, EG_OFF
	};

private:
	class Channel;

	class Slot {
	public:
		Slot();
		[[nodiscard]] inline int op_calc(unsigned phase, unsigned lfo_am) const;
		inline void FM_KEYON(byte key_set);
		inline void FM_KEYOFF(byte key_clr);
		inline void advanceEnvelopeGenerator(unsigned egCnt);
		inline void advancePhaseGenerator(Channel& ch, unsigned lfo_pm);
		void update_ar_dr();
		void update_rr();
		void calc_fc(const Channel& ch);

		/** Sets the amount of feedback [0..7]
		 */
		void setFeedbackShift(byte value) {
			fb_shift = value ? 9 - value : 0;
		}

		template<typename Archive>
		void serialize(Archive& ar, unsigned version);

		// Phase Generator
		FreqIndex Cnt;	// frequency counter
		FreqIndex Incr;	// frequency counter step
		int* connect;	// slot output pointer
		int op1_out[2];	// slot1 output for feedback

		// Envelope Generator
		unsigned TL;	// total level: TL << 2
		int TLL;	// adjusted now TL
		int volume;	// envelope counter
		int sl;		// sustain level: sl_tab[SL]

		const unsigned* wavetable; // waveform select

		EnvelopeState state; // EG: phase type
		unsigned eg_m_ar;// (attack state)
		unsigned eg_m_dr;// (decay state)
		unsigned eg_m_rr;// (release state)
		byte eg_sh_ar;	// (attack state)
		byte eg_sel_ar;	// (attack state)
		byte eg_sh_dr;	// (decay state)
		byte eg_sel_dr;	// (decay state)
		byte eg_sh_rr;	// (release state)
		byte eg_sel_rr;	// (release state)

		byte key;	// 0 = KEY OFF, >0 = KEY ON

		byte fb_shift;	// PG: feedback shift value
		bool CON;	// PG: connection (algorithm) type
		bool eg_type;	// EG: percussive/non-percussive mode

		// LFO
		byte AMmask;	// LFO Amplitude Modulation enable mask
		bool vib;	// LFO Phase Modulation enable flag (active high)

		byte ar;	// attack rate: AR<<2
		byte dr;	// decay rate:  DR<<2
		byte rr;	// release rate:RR<<2
		byte KSR;	// key scale rate
		byte ksl;	// keyscale level
		byte ksr;	// key scale rate: kcode>>KSR
		byte mul;	// multiple: mul_tab[ML]
	};

	class Channel {
	public:
		Channel();
		void chan_calc(unsigned lfo_am);
		void chan_calc_ext(unsigned lfo_am);

		template<typename Archive>
		void serialize(Archive& ar, unsigned version);

		Slot slot[2];

		int block_fnum;	// block+fnum
		FreqIndex fc;	// Freq. Increment base
		int ksl_base;	// KeyScaleLevel Base step
		byte kcode;	// key code (for key scaling)

		// there are 12 2-operator channels which can be combined in pairs
		// to form six 4-operator channel, they are:
		//  0 and 3,
		//  1 and 4,
		//  2 and 5,
		//  9 and 12,
		//  10 and 13,
		//  11 and 14
		bool extended; // set if this channel forms up a 4op channel with
			       // another channel (only used by first of pair of
			       // channels, ie 0,1,2 and 9,10,11)
	};

	// SoundDevice
	[[nodiscard]] float getAmplificationFactorImpl() const override;
	void generateChannels(float** bufs, unsigned num) override;

	void callback(byte flag) override;

	void writeRegDirect(unsigned r, byte v, EmuTime::param time);
	void init_tables();
	void setStatus(byte flag);
	void resetStatus(byte flag);
	void changeStatusMask(byte flag);
	void advance();

	[[nodiscard]] inline int genPhaseHighHat();
	[[nodiscard]] inline int genPhaseSnare();
	[[nodiscard]] inline int genPhaseCymbal();

	void chan_calc_rhythm(unsigned lfo_am);
	void set_mul(unsigned sl, byte v);
	void set_ksl_tl(unsigned sl, byte v);
	void set_ar_dr(unsigned sl, byte v);
	void set_sl_rr(unsigned sl, byte v);
	bool checkMuteHelper();

	[[nodiscard]] inline bool isExtended(unsigned ch) const;
	[[nodiscard]] inline Channel& getFirstOfPair(unsigned ch);
	[[nodiscard]] inline Channel& getSecondOfPair(unsigned ch);

	struct Debuggable final : SimpleDebuggable {
		Debuggable(MSXMotherBoard& motherBoard, const std::string& name);
		[[nodiscard]] byte read(unsigned address) override;
		void write(unsigned address, byte value, EmuTime::param time) override;
	} debuggable;

	// Bitmask for register 0x04
	static constexpr int R04_ST1       = 0x01; // Timer1 Start
	static constexpr int R04_ST2       = 0x02; // Timer2 Start
	static constexpr int R04_MASK_T2   = 0x20; // Mask Timer2 flag
	static constexpr int R04_MASK_T1   = 0x40; // Mask Timer1 flag
	static constexpr int R04_IRQ_RESET = 0x80; // IRQ RESET

	// Bitmask for status register
	static constexpr int STATUS_T2      = R04_MASK_T2;
	static constexpr int STATUS_T1      = R04_MASK_T1;
	// Timers (see EmuTimer class for details about timing)
	EmuTimer timer1; //  80.8us OPL4  ( 80.5us OPL3)
	EmuTimer timer2; // 323.1us OPL4  (321.8us OPL3)

	IRQHelper irq;

	int chanout[18]; // 18 channels

	byte reg[512];
	Channel channel[18];	// OPL3 chips have 18 channels

	unsigned pan[18 * 4];		// channels output masks 4 per channel
	                                //    0xffffffff = enable
	unsigned eg_cnt;		// global envelope generator counter
	unsigned noise_rng;		// 23 bit noise shift register

	// LFO
	using LFOAMIndex = FixedPoint< 6>;
	using LFOPMIndex = FixedPoint<10>;
	LFOAMIndex lfo_am_cnt;
	LFOPMIndex lfo_pm_cnt;
	bool lfo_am_depth;
	byte lfo_pm_depth_range;

	byte rhythm;			// Rhythm mode
	bool nts;			// NTS (note select)
	bool OPL3_mode;			// OPL3 extension enable flag

	byte status;			// status flag
	byte status2;
	byte statusMask;		// status mask

	bool alreadySignaledNEW2;
	const bool isYMF278;		// true iff this is actually a YMF278
					// ATM only used for NEW2 bit
};
SERIALIZE_CLASS_VERSION(YMF262, 2);

} // namespace openmsx

#endif
