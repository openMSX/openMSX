#ifndef MUSICMODULEMIDI_HH
#define MUSICMODULEMIDI_HH

#include "MSXDevice.hh"
#include "MC6850.hh"
#include "serialize_meta.hh"

namespace openmsx {

class MusicModuleMIDI final : public MSXDevice
{
public:
	explicit MusicModuleMIDI(const DeviceConfig& config);

	// MSXDevice
	void reset(EmuTime::param time) override;
	[[nodiscard]] byte readIO(word port, EmuTime::param time) override;
	[[nodiscard]] byte peekIO(word port, EmuTime::param time) const override;
	void writeIO(word port, byte value, EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	MC6850 mc6850;
};
SERIALIZE_CLASS_VERSION(MusicModuleMIDI, 4);

} // namespace openmsx

#endif
