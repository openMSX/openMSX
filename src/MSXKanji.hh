#ifndef MSXKANJI_HH
#define MSXKANJI_HH

#include "MSXDevice.hh"
#include "Rom.hh"

namespace openmsx {

class MSXKanji final : public MSXDevice
{
public:
	explicit MSXKanji(DeviceConfig& config);

	[[nodiscard]] byte readIO(uint16_t port, EmuTime::param time) override;
	[[nodiscard]] byte peekIO(uint16_t port, EmuTime::param time) const override;
	void writeIO(uint16_t port, byte value, EmuTime::param time) override;
	void reset(EmuTime::param time) override;

	void getExtraDeviceInfo(TclObject& result) const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	Rom rom;
	unsigned adr1, adr2;
	const bool isLascom;
	const byte highAddressMask;
};

} // namespace openmsx

#endif
