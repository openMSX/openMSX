// $Id$

#ifndef Y8950_HH
#define Y8950_HH

#include "SoundDevice.hh"
#include "EmuTimer.hh"
#include "Resample.hh"
#include "IRQHelper.hh"
#include "openmsx.hh"
#include <memory>

namespace openmsx {

class Y8950Adpcm;
class Y8950KeyboardConnector;
class Y8950Periphery;
class DACSound16S;
class MSXMotherBoard;
class Y8950Debuggable;

class Y8950 : public SoundDevice, private EmuTimerCallback, private Resample<1>
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
		void reset();

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
		int output[5];		// Output value of slot

		// for Phase Generator (PG)
		unsigned int phase;	// Phase
		unsigned int dphase;	// Phase increment amount
		int pgout;		// Output

		// for Envelope Generator (EG)
		int fnum;		// F-Number
		int block;		// Block
		int tll;		// Total Level + Key scale level
		int rks;		// Key scale offset (Rks)
		int eg_mode;		// Current state
		unsigned int eg_phase;	// Phase
		unsigned int eg_dphase; // Phase increment amount
		int egout;		// Output

		bool slotStatus;
		Patch patch;

		// refer to Y8950->
		int* plfo_pm;
		int* plfo_am;
	};

	class Channel {
	public:
		Channel();
		void reset();
		inline void setFnumber(int fnum);
		inline void setBlock(int block);
		inline void keyOn();
		inline void keyOff();

		bool alg;
		Slot mod, car;
	};

public:
	Y8950(MSXMotherBoard& motherBoard, const std::string& name,
	      const XMLElement& config, unsigned sampleRam, const EmuTime& time,
	      Y8950Periphery& perihery);
	virtual ~Y8950();

	void reset(const EmuTime &time);
	void writeReg(byte reg, byte data, const EmuTime& time);
	byte readReg(byte reg, const EmuTime& time);
	byte peekReg(byte reg, const EmuTime& time) const;
	byte readStatus();
	byte peekStatus() const;

	static const int CLOCK_FREQ = 3579545;

private:
	// SoundDevice
	virtual void setVolume(int maxVolume);
	virtual void setSampleRate(int sampleRate);
	virtual void updateBuffer(unsigned length, int* buffer,
		const EmuTime& start, const EmuDuration& sampDur);

	// Resample
	virtual void generateInput(float* buffer, unsigned num);

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

	void setStatus(byte flags);
	void resetStatus(byte flags);
	void changeStatusMask(byte newMask);

	void callback(byte flag);

	int adr;
	int output[2];
	// Registers
	byte reg[0x100];
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
	Slot* slot[18];

	int lfo_pm;
	int lfo_am;

	int maxVolume;

	// Bitmask for register 0x04
	// Timer1 Start.
	static const int R04_ST1          = 0x01;
	// Timer2 Start.
	static const int R04_ST2          = 0x02;
	// not used
	//static const int R04            = 0x04;
	// Mask 'Buffer Ready'.
	static const int R04_MASK_BUF_RDY = 0x08;
	// Mask 'End of sequence'.
	static const int R04_MASK_EOS     = 0x10;
	// Mask Timer2 flag.
	static const int R04_MASK_T2      = 0x20;
	// Mask Timer1 flag.
	static const int R04_MASK_T1      = 0x40;
	// IRQ RESET.
	static const int R04_IRQ_RESET    = 0x80;

	// Bitmask for status register
	static const int STATUS_EOS     = R04_MASK_EOS;
	static const int STATUS_BUF_RDY = R04_MASK_BUF_RDY;
	static const int STATUS_T2      = R04_MASK_T2;
	static const int STATUS_T1      = R04_MASK_T1;

	byte status;     // STATUS Register
	byte statusMask; // bit=0 -> masked
	IRQHelper irq;

	Y8950Periphery& perihery;

	// Timers
	EmuTimerOPL3_1 timer1; //  80us timer
	EmuTimerOPL3_2 timer2; // 320us timer

	// ADPCM
	const std::auto_ptr<Y8950Adpcm> adpcm;
	friend class Y8950Adpcm;

	// Keyboard connector
	const std::auto_ptr<Y8950KeyboardConnector> connector;

	// 13-bit (exponential) DAC
	const std::auto_ptr<DACSound16S> dac13;

	friend class Y8950Debuggable;
	const std::auto_ptr<Y8950Debuggable> debuggable;
};

} // namespace openmsx

#endif
