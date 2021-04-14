#include "Autofire.hh"
#include <algorithm>
#include <cassert>

namespace openmsx {

Autofire::Autofire(CommandController& commandController,
                   unsigned newMinInts, unsigned newMaxInts, std::string_view name)
	: min_ints(std::max(newMinInts, 1u))
	, max_ints(std::max(newMaxInts, min_ints + 1))
	, speedSetting(commandController, name,
		"controls the speed of this autofire circuit", 0, 0, 100)
	, clock(EmuTime::zero())
{
	setClock();
	speedSetting.attach(*this);
}

Autofire::~Autofire()
{
	speedSetting.detach(*this);
}

void Autofire::setClock()
{
	if (int speed = speedSetting.getInt()) {
		clock.setFreq(
		    (2 * 50 * 60) / (max_ints - (speed * (max_ints - min_ints)) / 100));
	} else {
		clock.setPeriod(EmuDuration::zero()); // special value: disabled
	}
}

void Autofire::update(const Setting& setting) noexcept
{
	(void)setting;
	assert(&setting == &speedSetting);
	setClock();
}

bool Autofire::getSignal(EmuTime::param time)
{
	return (clock.getPeriod() == EmuDuration::zero())
		? false // special value: disabled
		: (clock.getTicksTill(time) & 1);
}

template<typename Archive>
void Autofire::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("clock", clock);
}
INSTANTIATE_SERIALIZE_METHODS(Autofire)

} // namespace openmsx
