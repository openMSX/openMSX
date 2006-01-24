// $Id$

#include "Autofire.hh"
#include "IntegerSetting.hh"

using std::string;

namespace openmsx {

Autofire::Autofire(CommandController& commandController,
                   unsigned newMinInts, unsigned newMaxInts, const string& name)
	: min_ints(newMinInts)
	, max_ints(newMaxInts)
	, speedSetting(new IntegerSetting(commandController, name,
	               "controls the speed of this autofire circuit", 0, 0, 100))
	, clock(EmuTime::zero)
{
	if (min_ints < 1) {
		min_ints = 1;
	}
	if (max_ints <= min_ints) {
		max_ints = min_ints + 1;
	}
	setClock();

	speedSetting->attach(*this);
}

Autofire::~Autofire()
{
	speedSetting->detach(*this);
}

void Autofire::setClock()
{
	int speed = speedSetting->getValue();
	clock.setFreq(
	    (2 * 50 * 60) / (max_ints - (speed * (max_ints - min_ints)) / 100));
}

void Autofire::update(const Setting& setting)
{
	(void)setting;
	assert(&setting == speedSetting.get());
	setClock();
}

byte Autofire::getSignal(const EmuTime& time)
{
	return speedSetting->getValue() == 0 ? 0 : clock.getTicksTill(time) & 1;
}

} // namespace openmsx
