// $Id$

#include "FrontSwitch.hh"
#include "CommandController.hh"


FrontSwitch::FrontSwitch()
{
	position = true;	// TODO read from config
	CommandController::instance()->registerCommand(this, "frontswitch");
}

FrontSwitch::~FrontSwitch()
{
	CommandController::instance()->unregisterCommand(this, "frontswitch");
}

bool FrontSwitch::isOn() const
{
	return position;
}

void FrontSwitch::execute(const std::vector<std::string> &tokens,
                          const EmuTime &time)
{
	switch (tokens.size()) {
	case 2: 
		if (tokens[1] == "on") {
			position = true;
			break;
		}
		if (tokens[1] == "off") {
			position = false;
			break;
		}
		// fall through
	default:
		throw CommandException("Syntax error");
	}
}

void FrontSwitch::help(const std::vector<std::string> &tokens) const
{
	print("This command controls the front switch");
	print(" frontswitch: on/off");
}
