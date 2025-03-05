#ifndef DUMMYMIDIOUTDEVICE_HH
#define DUMMYMIDIOUTDEVICE_HH

#include "MidiOutDevice.hh"

namespace openmsx {

class DummyMidiOutDevice final : public MidiOutDevice
{
public:
	// SerialDataInterface (part)
	void recvByte(uint8_t value, EmuTime::param time) override;
	[[nodiscard]] std::string_view getDescription() const override;
	void plugHelper(Connector& connector, EmuTime::param time) override;
	void unplugHelper(EmuTime::param time) override;
};

} // namespace openmsx

#endif
