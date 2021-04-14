#include "Autofire.hh"
#include "MSXMotherBoard.hh"
#include "Scheduler.hh"
#include "StateChange.hh"
#include "StateChangeDistributor.hh"
#include <algorithm>
#include <cassert>

namespace openmsx {

class AutofireStateChange final : public StateChange
{
public:
	AutofireStateChange() = default; // for serialize
	AutofireStateChange(EmuTime::param time_, std::string name_, int value_)
		: StateChange(time_)
		, name(std::move(name_)), value(value_) {}
	[[nodiscard]] const std::string& getName() const { return name; }
	[[nodiscard]] int getValue() const { return value; }
	template<typename Archive> void serialize(Archive& ar, unsigned /*version*/)
	{
		ar.template serializeBase<StateChange>(*this);
		ar.serialize("name",    name,
		             "value",   value);
	}

private:
	std::string name;
	int value;
};
REGISTER_POLYMORPHIC_CLASS(StateChange, AutofireStateChange, "AutofireStateChange");


Autofire::Autofire(MSXMotherBoard& motherBoard,
                   unsigned newMinInts, unsigned newMaxInts, static_string_view name_)
	: scheduler(motherBoard.getScheduler())
	, stateChangeDistributor(motherBoard.getStateChangeDistributor())
	, name(name_)
	, min_ints(std::max(newMinInts, 1u))
	, max_ints(std::max(newMaxInts, min_ints + 1))
	, speedSetting(motherBoard.getCommandController(), name,
		"controls the speed of this autofire circuit", 0, 0, 100)
	, clock(scheduler.getCurrentTime())
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

void Autofire::setSpeed(EmuTime::param time)
{
	stateChangeDistributor.distributeNew(std::make_shared<AutofireStateChange>(
		time, std::string(name), speedSetting.getInt()));
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

void Autofire::signalStateChange(const std::shared_ptr<StateChange>& event)
{
	auto* as = dynamic_cast<AutofireStateChange*>(event.get());
	if (!as) return;
	if (as->getName() != name) return;

	setClock(as->getValue());
}

void Autofire::stopReplay(EmuTime::param time) noexcept
{
	setSpeed(time); // re-sync with current value of the setting
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
