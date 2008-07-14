// $Id$

#ifndef SETETRISDONGLE_HH
#define SETETRISDONGLE_HH

#include "JoystickDevice.hh"
#include "serialize_meta.hh"

namespace openmsx {

class SETetrisDongle : public JoystickDevice
{
public:
	SETetrisDongle();

	//Pluggable
	virtual const std::string& getName() const;
	virtual const std::string& getDescription() const;
	virtual void plugHelper(Connector& connector, const EmuTime& time);
	virtual void unplugHelper(const EmuTime& time);

	//JoystickDevice
	virtual byte read(const EmuTime& time);
	virtual void write(byte value, const EmuTime& time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	byte status;
};

REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, SETetrisDongle, "SETetrisDongle");

} // namespace openmsx

#endif
