// $Id$

#ifndef SETETRISDONGLE_HH
#define SETETRISDONGLE_HH

#include "JoystickDevice.hh"

namespace openmsx {

class SETetrisDongle : public JoystickDevice
{
public:
	SETetrisDongle();
	virtual ~SETetrisDongle();

	//Pluggable
	virtual const std::string& getName() const;
	virtual const std::string& getDescription() const;
	virtual void plugHelper(Connector* connector, const EmuTime& time);
	virtual void unplugHelper(const EmuTime& time);

	//JoystickDevice
	virtual byte read(const EmuTime& time);
	virtual void write(byte value, const EmuTime& time);

private:
	byte status;
};

} // namespace openmsx

#endif
