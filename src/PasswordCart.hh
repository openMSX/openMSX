//

#ifndef PASSWORDCART_HH
#define PASSWORDCART_HH

#include "MSXDevice.hh"

namespace openmsx {

class PasswordCart : public MSXDevice
{
public:
	PasswordCart(MSXMotherBoard& motherBoard, const XMLElement& config,
		     const EmuTime& time);

	virtual void reset(const EmuTime& time);
	virtual void writeIO(word port, byte value, const EmuTime& time);
	virtual byte readIO(word port, const EmuTime& time);

private:
	byte pointer;
	word password;
};

} // namespace

#endif
