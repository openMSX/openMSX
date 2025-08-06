#include "Autofire.hh"

#include "MSXMotherBoard.hh"
#include "Scheduler.hh"
#include "StateChange.hh"
#include "StateChangeDistributor.hh"

#include <algorithm>
#include <cassert>

namespace openmsx {

static std::string_view nameForId(Autofire::ID id)
{
	switch (id) {
		case Autofire::ID::RENSHATURBO: return "renshaturbo";
		default: return "unknown-autofire";
	}
}

class AutofireStateChange final : public StateChange
{
public:
	AutofireStateChange() = default; // for serialize
	AutofireStateChange(EmuTime time_, Autofire::ID id_, int value_)
		: StateChange(time_)
		, id(id_), value(value_) {}
	[[nodiscard]] auto getId() const { return id; }
	[[nodiscard]] int getValue() const { return value; }
	template<typename Archive> void serialize(Archive& ar, unsigned /*version*/)
	{
		ar.template serializeBase<StateChange>(*this);
		// for backwards compatibility serialize 'id' as 'name'
		std::string name = Archive::IS_LOADER ? "" : std::string(nameForId(id));
		ar.serialize("name",    name,
		             "value",   value);
		if constexpr (Archive::IS_LOADER) {
			id = (name == nameForId(Autofire::ID::RENSHATURBO))
			   ? Autofire::ID::RENSHATURBO
			   : Autofire::ID::UNKNOWN;
		}
	}

private:
	Autofire::ID id;
	int value;
};
REGISTER_POLYMORPHIC_CLASS(StateChange, AutofireStateChange, "AutofireStateChange");


Autofire::Autofire(MSXMotherBoard& motherBoard,
                   unsigned newMinInts, unsigned newMaxInts, ID id_)
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
	const auto* as = dynamic_cast<const AutofireStateChange*>(&event);
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
