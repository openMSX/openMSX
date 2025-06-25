#ifndef DUMMYMIDIOUTDEVICE_HH
#define DUMMYMIDIOUTDEVICE_HH

#include "MidiOutDevice.hh"

namespace openmsx {

class DummyMidiOutDevice final : public MidiOutDevice
{
public:
	// SerialDataInterface (part)
	void recvByte(uint8_t value, EmuTime time) override;
	[[nodiscard]] std::string_view getDescription() const override;
	void plugHelper(Connector& connector, EmuTime time) override;
	void unplugHelper(EmuTime time) override;
};

} // namespace openmsx

#endif
