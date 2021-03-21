#include "Paddle.hh"
#include "MSXEventDistributor.hh"
#include "StateChangeDistributor.hh"
#include "InputEvents.hh"
#include "StateChange.hh"
#include "checked_cast.hh"
#include "serialize.hh"
#include "serialize_meta.hh"
#include <algorithm>

namespace openmsx {

class PaddleState final : public StateChange
{
public:
	PaddleState() = default; // for serialize
	PaddleState(EmuTime::param time_, int delta_)
		: StateChange(time_), delta(delta_) {}
	[[nodiscard]] int getDelta() const { return delta; }

	template<typename Archive> void serialize(Archive& ar, unsigned /*version*/)
	{
		ar.template serializeBase<StateChange>(*this);
		ar.serialize("delta", delta);
	}
private:
	int delta;
};
REGISTER_POLYMORPHIC_CLASS(StateChange, PaddleState, "PaddleState");


Paddle::Paddle(MSXEventDistributor& eventDistributor_,
               StateChangeDistributor& stateChangeDistributor_)
	: eventDistributor(eventDistributor_)
	, stateChangeDistributor(stateChangeDistributor_)
	, lastPulse(EmuTime::zero())
	, analogValue(128)
	, lastInput(0)
{
}

Paddle::~Paddle()
{
	if (isPluggedIn()) {
		Paddle::unplugHelper(EmuTime::dummy());
	}
}


// Pluggable
std::string_view Paddle::getName() const
{
	return "paddle";
}

std::string_view Paddle::getDescription() const
{
	return "MSX Paddle";
}

void Paddle::plugHelper(Connector& /*connector*/, EmuTime::param /*time*/)
{
	eventDistributor.registerEventListener(*this);
	stateChangeDistributor.registerListener(*this);
}

void Paddle::unplugHelper(EmuTime::param /*time*/)
{
	stateChangeDistributor.unregisterListener(*this);
	eventDistributor.unregisterEventListener(*this);
}

// JoystickDevice
byte Paddle::read(EmuTime::param time)
{
	// The loop in the BIOS routine that reads the paddle status takes
	// 41 Z80 cycles per iteration.
	static constexpr auto TICK = EmuDuration::hz(3579545) * 41;

	assert(time >= lastPulse);
	bool before = (time - lastPulse) < (TICK * analogValue);
	bool output = before && !(lastInput & 4);
	return output ? 0x3F : 0x3E; // pin1 (up)
}

void Paddle::write(byte value, EmuTime::param time)
{
	byte diff = lastInput ^ value;
	lastInput = value;
	if ((diff & 4) && !(lastInput & 4)) { // high->low edge
		lastPulse = time;
	}
}

// MSXEventListener
void Paddle::signalMSXEvent(const std::shared_ptr<const Event>& event,
                            EmuTime::param time) noexcept
{
	if (event->getType() != OPENMSX_MOUSE_MOTION_EVENT) return;

	const auto& mev = checked_cast<const MouseMotionEvent&>(*event);
	constexpr int SCALE = 2;
	int delta = mev.getX() / SCALE;
	if (delta == 0) return;

	stateChangeDistributor.distributeNew(
		std::make_shared<PaddleState>(time, delta));
}

// StateChangeListener
void Paddle::signalStateChange(const std::shared_ptr<StateChange>& event)
{
	const auto* ps = dynamic_cast<const PaddleState*>(event.get());
	if (!ps) return;
	int newAnalog = analogValue + ps->getDelta();
	analogValue = std::min(std::max(newAnalog, 0), 255);
}

void Paddle::stopReplay(EmuTime::param /*time*/) noexcept
{
}

template<typename Archive>
void Paddle::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("lastPulse",   lastPulse,
	             "analogValue", analogValue,
	             "lastInput",   lastInput);

	if constexpr (ar.IS_LOADER) {
		if (isPluggedIn()) {
			plugHelper(*getConnector(), EmuTime::dummy());
		}
	}
}
INSTANTIATE_SERIALIZE_METHODS(Paddle);
REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, Paddle, "Paddle");

} // namespace openmsx
