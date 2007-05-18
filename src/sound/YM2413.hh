// $Id$

#ifndef YM2413_HH
#define YM2413_HH

#include "YM2413Core.hh"
#include "SoundDevice.hh"
#include "Resample.hh"
#include "openmsx.hh"
#include <memory>

namespace openmsx {

class EmuTime;
class MSXMotherBoard;
class YM2413Debuggable;

class YM2413 : public YM2413Core, public SoundDevice, private Resample
{
	struct Patch {
		Patch();
		Patch(int n, const byte* data);

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
		explicit Slot(bool type);
		void reset(bool type);

		inline void slotOn();
		inline void slotOn2();
		inline void slotOff();
		inline void setPatch(Patch* patch);
		inline void setVolume(int volume);
		inline void calc_phase(int lfo_pm);
		inline void calc_envelope(int lfo_am);
		inline int calc_slot_car(int fm);
		inline int calc_slot_mod();
		inline int calc_slot_tom();
		inline int calc_slot_snare(bool noise);
		inline int calc_slot_cym(unsigned pgout_hh);
		inline int calc_slot_hat(int pgout_cym, bool noise);
		inline void updatePG();
		inline void updateTLL();
		inline void updateRKS();
		inline void updateWF();
		inline void updateEG();
		inline void updateAll();

		Patch* patch;

		// OUTPUT
		int feedback;
		int output[5];		// Output value of slot

		// for Phase Generator (PG)
		word* sintbl;		// Wavetable
		unsigned phase;		// Phase
		unsigned dphase;	// Phase increment amount
		unsigned pgout;		// output

		// for Envelope Generator (EG)
		int fnum;		// F-Number
		int block;		// Block
		int volume;		// Current volume
		int sustine;		// Sustine 1 = ON, 0 = OFF
		int tll;		// Total Level + Key scale level
		int rks;		// Key scale offset (Rks)
		int eg_mode;		// Current state
		unsigned eg_phase;	// Phase
		unsigned eg_dphase;	// Phase increment amount
		unsigned egout;		// output

		bool type;		// 0 : modulator 1 : carrier
		bool slot_on_flag;
	};
	friend class Slot;

	class Channel {
	public:
		Channel();
		void reset();
		inline void setPatch(int num);
		inline void setSustine(bool sustine);
		inline void setVol(int volume);
		inline void setFnumber(int fnum);
		inline void setBlock(int block);
		inline void keyOn();
		inline void keyOff();

		Patch* patches;
		int patch_number;
		Slot mod, car;
	};

public:
	YM2413(MSXMotherBoard& motherBoard, const std::string& name,
	       const XMLElement& config, const EmuTime& time);
	virtual ~YM2413();

	void reset(const EmuTime& time);
	void writeReg(byte reg, byte value, const EmuTime& time);

private:
	// SoundDevice
	virtual void setOutputRate(unsigned sampleRate);
	virtual void generateChannels(int** bufs, unsigned num);
	virtual bool updateBuffer(unsigned length, int* buffer,
		const EmuTime& time, const EmuDuration& sampDur);

	// Resample
	virtual bool generateInput(int* buffer, unsigned num);

	inline int adjust(int x);
	inline void calcSample(int** bufs, unsigned sample);

	bool checkMuteHelper();

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
	inline void update_rhythm_mode();
	inline void update_key_status();

private:
	friend class YM2413Debuggable;
	const std::auto_ptr<YM2413Debuggable> debuggable;

	byte reg[0x40];

	// Pitch Modulator
	unsigned pm_phase;
	int lfo_pm;

	// Amp Modulator
	unsigned am_phase;
	int lfo_am;

	// Noise Generator
	int noise_seed;

	// Channel & Slot
	Channel ch[9];
	Slot* slot[18];

	// Voice Data
	Patch patches[19 * 2];

	// Empty voice data
	static Patch nullPatch;
};

} // namespace openmsx

#endif
