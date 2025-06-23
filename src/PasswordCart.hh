#ifndef PASSWORDCART_HH
#define PASSWORDCART_HH

#include "MSXDevice.hh"

namespace openmsx {

class PasswordCart final : public MSXDevice
{
public:
	explicit PasswordCart(const DeviceConfig& config);

	void reset(EmuTime::param time) override;
	void writeIO(uint16_t port, byte value, EmuTime::param time) override;
	[[nodiscard]] byte readIO(uint16_t port, EmuTime::param time) override;
	[[nodiscard]] byte peekIO(uint16_t port, EmuTime::param time) const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	const uint16_t password;
	byte pointer;
};

} // namespace

#endif
