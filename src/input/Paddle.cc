#include "Paddle.hh"
#include "MSXEventDistributor.hh"
#include "StateChangeDistributor.hh"
#include "Event.hh"
#include "StateChange.hh"
#include "serialize.hh"
#include "serialize_meta.hh"
#include <algorithm>

namespace openmsx {

class PaddleState final : public StateChange
{
public:
	PaddleState() = default; // for serialize
	PaddleState(EmuTime time_, int delta_)
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

void Paddle::plugHelper(Connector& /*connector*/, EmuTime /*time*/)
{
	eventDistributor.registerEventListener(*this);
	stateChangeDistributor.registerListener(*this);
}

void Paddle::unplugHelper(EmuTime /*time*/)
{
	stateChangeDistributor.unregisterListener(*this);
	eventDistributor.unregisterEventListener(*this);
}

// JoystickDevice
uint8_t Paddle::read(EmuTime time)
{
	// The loop in the BIOS routine that reads the paddle status takes
	// 41 Z80 cycles per iteration.
	static constexpr auto TICK = EmuDuration::hz(3579545) * 41;

	assert(time >= lastPulse);
	bool before = (time - lastPulse) < (TICK * analogValue);
	bool output = before && !(lastInput & 4);
	return output ? 0x3F : 0x3E; // pin1 (up)
}

void Paddle::write(uint8_t value, EmuTime time)
{
	uint8_t diff = lastInput ^ value;
	lastInput = value;
	if ((diff & 4) && !(lastInput & 4)) { // high->low edge
		lastPulse = time;
	}
}

// MSXEventListener
void Paddle::signalMSXEvent(const Event& event,
                            EmuTime time) noexcept
{
	visit(overloaded{
		[&](const MouseMotionEvent& e) {
			constexpr int SCALE = 2;
			if (int delta = e.getX() / SCALE) {
				stateChangeDistributor.distributeNew<PaddleState>(
					time, delta);
			}
		},
		[](const EventBase&) { /*ignore*/ }
	}, event);
}

// StateChangeListener
void Paddle::signalStateChange(const StateChange& event)
{
	const auto* ps = dynamic_cast<const PaddleState*>(&event);
	if (!ps) return;
	analogValue = narrow_cast<uint8_t>(std::clamp(analogValue + ps->getDelta(), 0, 255));
}

void Paddle::stopReplay(EmuTime /*time*/) noexcept
{
}

template<typename Archive>
void Paddle::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("lastPulse",   lastPulse,
	             "analogValue", analogValue,
	             "lastInput",   lastInput);

	if constexpr (Archive::IS_LOADER) {
		if (isPluggedIn()) {
			plugHelper(*getConnector(), EmuTime::dummy());
		}
	}
}
INSTANTIATE_SERIALIZE_METHODS(Paddle);
REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, Paddle, "Paddle");

} // namespace openmsx
