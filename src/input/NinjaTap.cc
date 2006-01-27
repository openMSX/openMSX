// $Id$

#include "NinjaTap.hh"
#include "JoyTapPort.hh"
#include "DummyJoystick.hh"
#include "PluggingController.hh"
#include "CommandController.hh"


using std::string;

namespace openmsx {

NinjaTap::NinjaTap(CommandController& commandController,
		PluggingController& pluggingController_,
		const string& name_
	      )
	: JoyTap(commandController,pluggingController_,name_),
	lastValue(255) // != 0
{
}

NinjaTap::~NinjaTap()
{
}

const string& NinjaTap::getDescription() const
{
	static const string desc("MSX NinjaTap device.");
	return desc;
}

byte NinjaTap::read(const EmuTime& time)
{
	// TODO: actually implemt the ninja tap logic ! 
	byte value=255;
	for (int i=0 ; i<4 ; ++i){
		value |= slaves[i]->read(time);
	};
	return value;
}

void NinjaTap::write(byte value, const EmuTime& time)
{
	// TODO: actually implemt the ninja tap logic ! 
	if (lastValue != value) {
		lastValue = value;
		for (int i=0 ; i<4 ; ++i){
			slaves[i]->write(value,time);
		}
	}
}

} // namespace openmsx

