#ifndef VLM5030_HH
#define VLM5030_HH

#include "ResampledSoundDevice.hh"
#include "Rom.hh"
#include "EmuTime.hh"
#include "openmsx.hh"
#include <string>

namespace openmsx {

class DeviceConfig;

class VLM5030 final : public ResampledSoundDevice
{
public:
	VLM5030(const std::string& name, const static_string_view desc,
	        const std::string& romFilename, const DeviceConfig& config);
	~VLM5030();
	void reset();

	/** latch control data */
	void writeData(byte data);

	/** set RST / VCU / ST pins */
	void writeControl(byte data, EmuTime::param time);

	/** get BSY pin level */
	[[nodiscard]] bool getBSY(EmuTime::param time) const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void setRST(bool pin);
	void setVCU(bool pin);
	void setST (bool pin);

	// SoundDevice
	void generateChannels(float** bufs, unsigned num) override;
	[[nodiscard]] float getAmplificationFactorImpl() const override;

	void setupParameter(byte param);
	[[nodiscard]] int getBits(unsigned sBit, unsigned bits);
	[[nodiscard]] int parseFrame();

private:
	Rom rom;
	int address_mask;

	// state of option paramter
	int frame_size;
	int pitch_offset;

	// these contain data describing the current and previous voice frames
	// these are all used to contain the current state of the sound generation
	unsigned current_energy;
	unsigned current_pitch;
	int current_k[10];
	int x[10];

	word address;
	word vcu_addr_h;

	int16_t old_k[10];
	int16_t new_k[10];
	int16_t target_k[10];
	word old_energy;
	word new_energy;
	word target_energy;
	byte old_pitch;
	byte new_pitch;
	byte target_pitch;

	byte interp_step;
	byte interp_count; // number of interp periods
	byte sample_count; // sample number within interp
	byte pitch_count;

	byte latch_data;
	byte parameter;
	byte phase;
	bool pin_BSY;
	bool pin_ST;
	bool pin_VCU;
	bool pin_RST;
};

} // namespace openmsx

#endif
