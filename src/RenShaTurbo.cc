// $Id$

#include <cstdio>
#include <sstream>

#include "RenShaTurbo.hh"
#include "CommandController.hh"
#include "MSXConfig.hh"

namespace openmsx {

RenShaTurbo* RenShaTurbo::instance()
{
	static RenShaTurbo* oneInstance = NULL;
	if (oneInstance == NULL)
		oneInstance = new RenShaTurbo();
	return oneInstance;
}

RenShaTurbo::RenShaTurbo()
{
	is_present = false;
	try
	{
		Config *config = MSXConfig::instance()->getConfigById("RenShaTurbo");
       		if (config != NULL) {
			int min_ints=atoi(config->getParameter("min_ints").c_str());
			int max_ints=atoi(config->getParameter("max_ints").c_str());
			autofire = new Autofire(min_ints, max_ints);
			CommandController::instance()->registerCommand(&speedCmd, "renshaturbo");
			is_present = true; 
		}
	}
	catch (MSXException &e) {
		// Do nothing
	}
}

RenShaTurbo::~RenShaTurbo()
{
	if (is_present) {
		CommandController::instance()->unregisterCommand(&speedCmd, "renshaturbo");
		delete autofire;
	}
}

byte RenShaTurbo::getSignal(const EmuTime &time)
{
	if (is_present)
		return autofire->getSignal(time);
	else
		return 0;
}

void RenShaTurbo::increaseSpeed()
{
	autofire->increaseSpeed();
}

void RenShaTurbo::decreaseSpeed()
{
	autofire->decreaseSpeed();
}

int RenShaTurbo::getSpeed()
{
	return autofire->getSpeed();
}

void RenShaTurbo::setSpeed(const int newSpeed)
{
	autofire->setSpeed(newSpeed);
}

string RenShaTurbo::SpeedCmd::execute(const std::vector<std::string> &tokens)
{
	string result;
	RenShaTurbo *renshaturbo = RenShaTurbo::instance();

        int nrTokens = tokens.size();
	if (nrTokens == 1) {
		std::ostringstream out;
		out << "RenShaTurbo speed: " << renshaturbo->getSpeed();
		result += out.str() + '\n';
	}
	else if (nrTokens == 2) {
	        const std::string &param = tokens[1];
       		if (param == "+")
			renshaturbo->increaseSpeed();
		else if (param == "-")
			renshaturbo->decreaseSpeed();
		else
			renshaturbo->setSpeed(atoi(param.c_str()));
	}
	else {
                throw CommandException("Wrong number of parameters");
	}
	return result;
}

string RenShaTurbo::SpeedCmd::help(const std::vector<std::string> &tokens) const
{
	return "renshaturbo   : show current speed\n"
	       "renshaturbo + : increase the speed\n"
	       "renshaturbo - : decrease the speed\n"
	       "renshaturbo N : set speed to N (0 is off, 1 to 100 is speed)\n";
}

} // namespace openmsx
