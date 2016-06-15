#ifndef YMF278_HH
#define YMF278_HH

#include "ResampledSoundDevice.hh"
#include "SimpleDebuggable.hh"
#include "Rom.hh"
#include "MemBuffer.hh"
#include "EmuTime.hh"
#include "openmsx.hh"
#include "serialize_meta.hh"
#include <string>

namespace openmsx {

class DeviceConfig;

class YMF278 final : public ResampledSoundDevice
{
public:
	YMF278(const std::string& name, int ramSize,
	       const DeviceConfig& config);
	~YMF278();
	void clearRam();
	void reset(EmuTime::param time);
	void writeReg(byte reg, byte data, EmuTime::param time);
	byte readReg(byte reg);
	byte peekReg(byte reg) const;

	byte readMem(unsigned address) const;
	void writeMem(unsigned address, byte value);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	class Slot {
	public:
		Slot();
		void reset();
		int compute_rate(int val) const;
		unsigned decay_rate(int num, int sample_rate);
		void envelope_next(int sample_rate);
		inline int compute_vib() const;
		inline int compute_am() const;
		void set_lfo(int newlfo);

		template<typename Archive>
		void serialize(Archive& ar, unsigned version);

		unsigned startaddr;
		unsigned loopaddr;
		unsigned endaddr;
		unsigned step;       // fixed-point frequency step
				     // invariant: step == calcStep(OCT, FN)
		unsigned stepptr;    // fixed-point pointer into the sample
		unsigned pos;
		int16_t sample1, sample2;

		int env_vol;

		int lfo_cnt;
		int lfo_step;
		int lfo_max;

		int DL;
		int16_t wave;		// wavetable number
		int16_t FN;		// f-number         TODO store 'FN | 1024'?
		char OCT;		// octave [0..15]   TODO store sign-extended?
		char PRVB;		// pseudo-reverb
		char LD;		// level direct
		char TL;		// total level
		char pan;		// panpot
		char lfo;		// LFO
		char vib;		// vibrato
		char AM;		// AM level
		char AR;
		char D1R;
		char D2R;
		char RC;		// rate correction
		char RR;

		byte bits;		// width of the samples
		bool active;		// slot keyed on

		byte state;
		bool lfo_active;
	};

	// SoundDevice
	void generateChannels(int** bufs, unsigned num) override;

	void writeRegDirect(byte reg, byte data, EmuTime::param time);
	unsigned getRamAddress(unsigned addr) const;
	int16_t getSample(Slot& op);
	void advance();
	bool anyActive();
	void keyOnHelper(Slot& slot);

	MSXMotherBoard& motherBoard;

	struct DebugRegisters final : SimpleDebuggable {
		DebugRegisters(MSXMotherBoard& motherBoard, const std::string& name);
		byte read(unsigned address) override;
		void write(unsigned address, byte value, EmuTime::param time) override;
	} debugRegisters;

	struct DebugMemory final : SimpleDebuggable {
		DebugMemory(MSXMotherBoard& motherBoard, const std::string& name);
		byte read(unsigned address) override;
		void write(unsigned address, byte value) override;
	} debugMemory;

	Slot slots[24];

	/** Global envelope generator counter. */
	unsigned eg_cnt;

	int memadr;

	int fm_l, fm_r;
	int pcm_l, pcm_r;

	Rom rom;
	const unsigned ramSize;
	MemBuffer<byte> ram;

	/** Precalculated attenuation values with some margin for
	  * envelope and pan levels.
	  */
	int volume[256 * 4];

	byte regs[256];
};
SERIALIZE_CLASS_VERSION(YMF278::Slot, 3);
SERIALIZE_CLASS_VERSION(YMF278, 3);

} // namespace openmsx

#endif
