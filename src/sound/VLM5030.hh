#ifndef VLM5030_HH
#define VLM5030_HH

#include "ResampledSoundDevice.hh"

#include "EmuTime.hh"
#include "Rom.hh"

#include <array>
#include <cstdint>
#include <string>

namespace openmsx {

class DeviceConfig;

class VLM5030 final : public ResampledSoundDevice
{
public:
	VLM5030(const std::string& name, static_string_view desc,
	        std::string_view romFilename, const DeviceConfig& config);
	~VLM5030();
	void reset();

	/** latch control data */
	void writeData(uint8_t data);

	/** set RST / VCU / ST pins */
	void writeControl(uint8_t data, EmuTime::param time);

	/** get BSY pin level */
	[[nodiscard]] bool getBSY(EmuTime::param time) const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void setRST(bool pin);
	void setVCU(bool pin);
	void setST (bool pin);

	// SoundDevice
	void generateChannels(std::span<float*> bufs, unsigned num) override;
	[[nodiscard]] float getAmplificationFactorImpl() const override;

	void setupParameter(uint8_t param);
	[[nodiscard]] unsigned getBits(unsigned sBit, unsigned bits) const;
	[[nodiscard]] int parseFrame();

private:
	Rom rom;
	unsigned address_mask;

	// state of option parameter
	uint8_t frame_size;
	int pitch_offset;

	// these contain data describing the current and previous voice frames
	// these are all used to contain the current state of the sound generation
	unsigned current_energy;
	unsigned current_pitch;
	std::array<int, 10> current_k;
	std::array<int, 10> x;

	uint16_t address;
	uint16_t vcu_addr_h;

	std::array<int16_t, 10> old_k;
	std::array<int16_t, 10> new_k;
	std::array<int16_t, 10> target_k;
	uint16_t old_energy;
	uint16_t new_energy;
	uint16_t target_energy;
	uint8_t old_pitch;
	uint8_t new_pitch;
	uint8_t target_pitch;

	uint8_t interp_step;
	uint8_t interp_count; // number of interp periods
	uint8_t sample_count; // sample number within interp
	uint8_t pitch_count;

	uint8_t latch_data{0};
	uint8_t parameter;
	uint8_t phase;
	bool pin_BSY{false};
	bool pin_ST{false};
	bool pin_VCU{false};
	bool pin_RST{false};
};

} // namespace openmsx

#endif
