// $Id$

#include <cstdio>
#include <sstream>

#include "Autofire.hh"
#include "MSXConfig.hh"

namespace openmsx {

Autofire::Autofire(int newMinInts, int newMaxInts)
{
	min_ints=newMinInts;
	if (min_ints < 1)
		min_ints = 1;
	max_ints=newMaxInts;
	if (max_ints < min_ints)
		max_ints = min_ints + 1;
			
	speed = 0; 
	old_speed = 0;
}

Autofire::~Autofire()
{
}

byte Autofire::getSignal(const EmuTime &time)
{
	if (speed != 0) {
		if (speed != old_speed) {
			freq_divider = (MAIN_FREQ * (max_ints - (speed * (max_ints - min_ints))/100)) / (2 * 50 * 60);
			old_speed = speed;
		}
		return (time.time / freq_divider) & 1;
        }
	else
		return 0;
}

void Autofire::increaseSpeed()
{
	if (speed < 100)
		speed++;
}

void Autofire::decreaseSpeed()
{
	if (speed != 0)
		speed--;
}

int Autofire::getSpeed()
{
	return speed;
}

void Autofire::setSpeed(const int newSpeed)
{
	if (newSpeed < 0)
		speed = 0;
	else if (newSpeed > 100)
		speed = 100;
	else
		speed = newSpeed;
}

/*
void Autofire::SpeedCmd::execute(const std::vector<std::string> &tokens)
{
	Autofire *autof = Autofire::instance();

        int nrTokens = tokens.size();
	if (nrTokens == 1) {
		std::ostringstream out;
		out << "Autofire speed: " << autof->getSpeed();
		print(out.str());
	}
	else if (nrTokens == 2) {
	        const std::string &param = tokens[1];
       		if (param == "+")
			autof->increaseSpeed();
		else if (param == "-")
			autof->decreaseSpeed();
		else
			autof->setSpeed(atoi(param.c_str()));
	}
	else {
                throw CommandException("Wrong number of parameters");
	}

}
*/

/*
void Autofire::SpeedCmd::help(const std::vector<std::string> &tokens) const
{
	print("autofire   : show current speed");
        print("autofire + : increase the speed");
        print("autofire - : decrease the speed");
        print("autofire N : set speed to N (0 is off, 1 to 100 is speed)");
}
*/

} // namespace openmsx
