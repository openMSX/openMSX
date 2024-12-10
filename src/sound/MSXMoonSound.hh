#ifndef MSXMOONSOUND_HH
#define MSXMOONSOUND_HH

#include "MSXDevice.hh"
#include "YMF278B.hh"
#include "serialize_meta.hh"

namespace openmsx {

class MSXMoonSound final : public MSXDevice
{
public:
	explicit MSXMoonSound(const DeviceConfig& config);

	void powerUp(EmuTime::param time) override;
	void reset(EmuTime::param time) override;
	[[nodiscard]] byte readIO(word port, EmuTime::param time) override;
	[[nodiscard]] byte peekIO(word port, EmuTime::param time) const override;
	void writeIO(word port, byte value, EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	YMF278B ymf278b;
};
SERIALIZE_CLASS_VERSION(MSXMoonSound, 4);

} // namespace openmsx

#endif
