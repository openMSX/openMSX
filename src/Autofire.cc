#include "Autofire.hh"

#include "MSXMotherBoard.hh"
#include "Scheduler.hh"
#include "StateChange.hh"
#include "StateChangeDistributor.hh"

#include <algorithm>
#include <cassert>

namespace openmsx {

Autofire::Autofire(MSXMotherBoard& motherBoard,
                   unsigned newMinInts, unsigned newMaxInts, AutofireID id_)
	: scheduler(motherBoard.getScheduler())
	, stateChangeDistributor(motherBoard.getStateChangeDistributor())
	, min_ints(std::max(newMinInts, 1u))
	, max_ints(std::max(newMaxInts, min_ints + 1))
	, speedSetting(motherBoard.getCommandController(), nameForId(id_),
		"controls autofire speed (0 = disabled)", 0, 0, 100)
	, clock(scheduler.getCurrentTime())
	, id(id_)
{
	setClock(speedSetting.getInt());

	stateChangeDistributor.registerListener(*this);
	speedSetting.attach(*this);
}

Autofire::~Autofire()
{
	speedSetting.detach(*this);
	stateChangeDistributor.unregisterListener(*this);
}

void Autofire::setSpeed(EmuTime time)
{
	stateChangeDistributor.distributeNew<AutofireStateChange>(
		time, id, speedSetting.getInt());
}

void Autofire::setClock(int speed)
{
	if (speed) {
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
	setSpeed(scheduler.getCurrentTime());
}

void Autofire::signalStateChange(const StateChange& event)
{
	const auto* as = std::get_if<AutofireStateChange>(&event);
	if (!as) return;
	if (as->getId() != id) return;

	setClock(as->getValue());
}

void Autofire::stopReplay(EmuTime time) noexcept
{
	setSpeed(time); // re-sync with current value of the setting
}

bool Autofire::getSignal(EmuTime time) const
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
