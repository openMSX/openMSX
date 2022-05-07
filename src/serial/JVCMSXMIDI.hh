#ifndef JVCMSXMIDI_HH
#define JVCMSXMIDI_HH

#include "MSXDevice.hh"
#include "MC6850.hh"

namespace openmsx {

class JVCMSXMIDI final : public MSXDevice
{
public:
	explicit JVCMSXMIDI(const DeviceConfig& config);

	// MSXDevice
	void reset(EmuTime::param time) override;
	[[nodiscard]] byte readIO(word port, EmuTime::param time) override;
	[[nodiscard]] byte peekIO(word port, EmuTime::param time) const override;
	void writeIO(word port, byte value, EmuTime::param time) override;

	void serialize(Archive auto& ar, unsigned version);

private:
	MC6850 mc6850;
};

} // namespace openmsx

#endif
