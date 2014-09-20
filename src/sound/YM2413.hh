#ifndef YM2413_HH
#define YM2413_HH

#include "ResampledSoundDevice.hh"
#include "EmuTime.hh"
#include "openmsx.hh"
#include <memory>
#include <string>

namespace openmsx {

class YM2413Core;
class YM2413Debuggable;

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
	virtual void generateChannels(int** bufs, unsigned num);
	virtual int getAmplificationFactor() const;

	const std::unique_ptr<YM2413Core> core;
	const std::unique_ptr<YM2413Debuggable> debuggable;
	friend class YM2413Debuggable;
};

} // namespace openmsx

#endif
