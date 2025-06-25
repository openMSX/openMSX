#ifndef DUMMYMIDIINDEVICE_HH
#define DUMMYMIDIINDEVICE_HH

#include "MidiInDevice.hh"

namespace openmsx {

class DummyMidiInDevice final : public MidiInDevice
{
public:
	void signal(EmuTime time) override;
	[[nodiscard]] std::string_view getDescription() const override;
	void plugHelper(Connector& connector, EmuTime time) override;
	void unplugHelper(EmuTime time) override;
};

} // namespace openmsx

#endif
