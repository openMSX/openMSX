#ifndef YM2413_HH
#define YM2413_HH

#include "ResampledSoundDevice.hh"

#include "EmuTime.hh"
#include "SimpleDebuggable.hh"

#include <cstdint>
#include <memory>
#include <span>
#include <string>

namespace openmsx {

class YM2413Core;

class YM2413 final : public ResampledSoundDevice
{
public:
	/** YM2413: the MSX-MUSIC chip, with the core the config asks for.
	  * Y8960: the OPLL-EX of HRA!'s Y8960 cartridge, a YM2413 with a tone set
	  *        per channel (registers 0x40-0x48). Always the NukeYKT core.
	  */
	enum class Type : uint8_t { YM2413, Y8960 };

	YM2413(const std::string& name, const DeviceConfig& config,
	       Type type = Type::YM2413);
	~YM2413();

	void reset(EmuTime time);
	void writePort(bool port, uint8_t value, EmuTime time);
	void pokeReg(uint8_t reg, uint8_t value, EmuTime time);
	[[nodiscard]] std::span<const uint8_t, 64> peekRegs() const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// SoundDevice
	void setOutputRate(unsigned hostSampleRate, double speed) override;
	void generateChannels(std::span<float*> bufs, unsigned num) override;
	[[nodiscard]] float getAmplificationFactorImpl() const override;

private:
	const std::unique_ptr<YM2413Core> core;

	struct Debuggable final : SimpleDebuggable {
		Debuggable(MSXMotherBoard& motherBoard, const std::string& name,
		           static_string_view description);
		[[nodiscard]] uint8_t read(unsigned address) override;
		void write(unsigned address, uint8_t value, EmuTime time) override;
	} debuggable;
};

} // namespace openmsx

#endif
