#ifndef PASSWORDCART_HH
#define PASSWORDCART_HH

#include "MSXDevice.hh"

namespace openmsx {

class PasswordCart final : public MSXDevice
{
public:
	explicit PasswordCart(const DeviceConfig& config);

	void reset(EmuTime::param time) override;
	void writeIO(word port, byte value, EmuTime::param time) override;
	[[nodiscard]] byte readIO(word port, EmuTime::param time) override;
	[[nodiscard]] byte peekIO(word port, EmuTime::param time) const override;

	void serialize(Archive auto& ar, unsigned version);

private:
	const word password;
	byte pointer;
};

} // namespace

#endif
