#ifndef DUMMYAUDIOINPUTDEVICE_HH
#define DUMMYAUDIOINPUTDEVICE_HH

#include "AudioInputDevice.hh"

namespace openmsx {

class DummyAudioInputDevice final : public AudioInputDevice
{
public:
	[[nodiscard]] zstring_view getDescription() const override;
	void plugHelper(Connector& connector, EmuTime time) override;
	void unplugHelper(EmuTime time) override;
	[[nodiscard]] int16_t readSample(EmuTime time) override;
};

} // namespace openmsx

#endif
