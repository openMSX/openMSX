#ifndef DUMMYMIDIINDEVICE_HH
#define DUMMYMIDIINDEVICE_HH

#include "MidiInDevice.hh"

namespace openmsx {

class DummyMidiInDevice final : public MidiInDevice
{
public:
	void signal(EmuTime::param time) override;
	[[nodiscard]] std::string_view getDescription() const override;
	void plugHelper(Connector& connector, EmuTime::param time) override;
	void unplugHelper(EmuTime::param time) override;
};

} // namespace openmsx

#endif
