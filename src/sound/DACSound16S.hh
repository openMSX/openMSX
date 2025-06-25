// This class implements a 16 bit signed DAC

#ifndef DACSOUND16S_HH
#define DACSOUND16S_HH

#include "SoundDevice.hh"
#include "BlipBuffer.hh"
#include <cstdint>

namespace openmsx {

class DACSound16S : public SoundDevice
{
public:
	DACSound16S(std::string_view name, static_string_view desc,
	            const DeviceConfig& config);
	virtual ~DACSound16S();

	void reset(EmuTime time);
	void writeDAC(int16_t value, EmuTime time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// SoundDevice
	void setOutputRate(unsigned hostSampleRate, double speed) override;
	void generateChannels(std::span<float*> bufs, unsigned num) override;
	bool updateBuffer(size_t length, float* buffer,
	                  EmuTime time) override;

private:
	BlipBuffer blip;
	int16_t lastWrittenValue = 0;
};

} // namespace openmsx

#endif
