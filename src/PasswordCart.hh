#ifndef PASSWORDCART_HH
#define PASSWORDCART_HH

#include "MSXDevice.hh"

namespace openmsx {

class PasswordCart final : public MSXDevice
{
public:
	explicit PasswordCart(const DeviceConfig& config);

	void reset(EmuTime time) override;
	void writeIO(uint16_t port, byte value, EmuTime time) override;
	[[nodiscard]] byte readIO(uint16_t port, EmuTime time) override;
	[[nodiscard]] byte peekIO(uint16_t port, EmuTime time) const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	const uint16_t password;
	byte pointer;
};

} // namespace

#endif
