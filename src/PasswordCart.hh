// $Id$

#ifndef PASSWORDCART_HH
#define PASSWORDCART_HH

#include "MSXDevice.hh"

namespace openmsx {

class PasswordCart : public MSXDevice
{
public:
	PasswordCart(MSXMotherBoard& motherBoard, const DeviceConfig& config);

	virtual void reset(EmuTime::param time);
	virtual void writeIO(word port, byte value, EmuTime::param time);
	virtual byte readIO(word port, EmuTime::param time);
	virtual byte peekIO(word port, EmuTime::param time) const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	const word password;
	byte pointer;
};

} // namespace

#endif
