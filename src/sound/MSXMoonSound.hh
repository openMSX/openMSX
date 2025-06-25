#ifndef MSXMOONSOUND_HH
#define MSXMOONSOUND_HH

#include "MSXDevice.hh"
#include "YMF278B.hh"
#include "serialize_meta.hh"

namespace openmsx {

class MSXMoonSound final : public MSXDevice
{
public:
	explicit MSXMoonSound(DeviceConfig& config);

	void powerUp(EmuTime time) override;
	void reset(EmuTime time) override;
	[[nodiscard]] byte readIO(uint16_t port, EmuTime time) override;
	[[nodiscard]] byte peekIO(uint16_t port, EmuTime time) const override;
	void writeIO(uint16_t port, byte value, EmuTime time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	YMF278B ymf278b;
};
SERIALIZE_CLASS_VERSION(MSXMoonSound, 4);

} // namespace openmsx

#endif
