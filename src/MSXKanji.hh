#ifndef MSXKANJI_HH
#define MSXKANJI_HH

#include "MSXDevice.hh"
#include "Rom.hh"

namespace openmsx {

class MSXKanji final : public MSXDevice
{
public:
	explicit MSXKanji(const DeviceConfig& config);

	byte readIO(word port, EmuTime::param time) override;
	byte peekIO(word port, EmuTime::param time) const override;
	void writeIO(word port, byte value, EmuTime::param time) override;
	void reset(EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	Rom rom;
	unsigned adr1, adr2;
	const bool isLascom;
};

} // namespace openmsx

#endif
