#ifndef YM2413_HH
#define YM2413_HH

#include "ResampledSoundDevice.hh"
#include "SimpleDebuggable.hh"
#include "EmuTime.hh"
#include "openmsx.hh"
#include <memory>
#include <string>

namespace openmsx {

class YM2413Core;

class YM2413 final : public ResampledSoundDevice
{
public:
	YM2413(const std::string& name, const DeviceConfig& config);
	~YM2413();

	void reset(EmuTime::param time);
	void writeReg(byte reg, byte value, EmuTime::param time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// SoundDevice
	void generateChannels(int** bufs, unsigned num) override;
	int getAmplificationFactor() const override;

	const std::unique_ptr<YM2413Core> core;

	class Debuggable final : public SimpleDebuggable {
	public:
		Debuggable(MSXMotherBoard& motherBoard, YM2413& ym2413);
		byte read(unsigned address) override;
		void write(unsigned address, byte value, EmuTime::param time) override;
	private:
		YM2413& ym2413;
	} debuggable;
};

} // namespace openmsx

#endif
