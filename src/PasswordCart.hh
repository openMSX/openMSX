// $Id$

#ifndef PASSWORDCART_HH
#define PASSWORDCART_HH

#include "MSXDevice.hh"

namespace openmsx {

class PasswordCart : public MSXDevice
{
public:
	PasswordCart(MSXMotherBoard& motherBoard, const XMLElement& config);

	virtual void reset(const EmuTime& time);
	virtual void writeIO(word port, byte value, const EmuTime& time);
	virtual byte readIO(word port, const EmuTime& time);
	virtual byte peekIO(word port, const EmuTime& time) const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	const word password;
	byte pointer;
};

} // namespace

#endif
