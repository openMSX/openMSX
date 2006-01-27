// $Id$

#include "JoyTap.hh"
#include "JoyTapPort.hh"
#include "DummyJoystick.hh"
#include "PluggingController.hh"
#include "CommandController.hh"

using std::string;

namespace openmsx {

JoyTap::JoyTap(CommandController& commandController,
		PluggingController& pluggingController_,
		const std::string& name_
	      )
	: name(name_), lastValue(255) // != 0
{
	for (int i=0 ; i<4 ; ++i){
		slaves[i]=new JoyTapPort(pluggingController_,getName()+"_port_"+char('1'+i));	
	}
}

JoyTap::~JoyTap()
{
	
}

const string& JoyTap::getDescription() const
{
	static const string desc("MSX JoyTap device.");
	return desc;
}

const string& JoyTap::getName() const
{
	return name;
}

void JoyTap::plugHelper(Connector& /*connector*/, const EmuTime& time)
{
}

void JoyTap::unplugHelper(const EmuTime& time)
{
}

byte JoyTap::read(const EmuTime& time)
{
	byte value=255;
	for (int i=0 ; i<4 ; ++i){
		value &= slaves[i]->read(time);
	};
	return value;
}

void JoyTap::write(byte value, const EmuTime& time)
{
	if (lastValue != value) {
		lastValue = value;
		for (int i=0 ; i<4 ; ++i){
			slaves[i]->write(value,time);
		}
	}
}

} // namespace openmsx

