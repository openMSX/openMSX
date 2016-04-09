#include "ArkanoidPad.hh"
#include "MSXEventDistributor.hh"
#include "StateChangeDistributor.hh"
#include "InputEvents.hh"
#include "StateChange.hh"
#include "checked_cast.hh"
#include "serialize.hh"
#include "serialize_meta.hh"
#include <algorithm>

// Implemented mostly according to the info here: http://www.msx.org/forumtopic7661.html
// This is absolutely not accurate, but good enough to make the pad work in the
// Arkanoid games.

// The hardware works with a 556 dual timer that's unemulated here, it requires
// short delays at each shift, and a long delay at latching. Too short delays
// will cause garbage reads, and too long delays at shifting will eventually
// cause the shift register bits to return to 0.

using std::string;
using std::shared_ptr;
using std::make_shared;

namespace openmsx {

static const int POS_MIN = 55; // measured by hap
static const int POS_MAX = 325; // measured by hap
static const int POS_CENTER = 236; // approx. middle used by games
static const int SCALE = 2;


class ArkanoidState final : public StateChange
{
public:
	ArkanoidState() {} // for serialize
	ArkanoidState(EmuTime::param time_, int delta_, bool press_, bool release_)
		: StateChange(time_)
		, delta(delta_), press(press_), release(release_) {}
	int  getDelta()   const { return delta; }
	bool getPress()   const { return press; }
	bool getRelease() const { return release; }

	template<typename Archive> void serialize(Archive& ar, unsigned /*version*/)
	{
		ar.template serializeBase<StateChange>(*this);
		ar.serialize("delta", delta);
		ar.serialize("press", press);
		ar.serialize("release", release);
	}
private:
	int delta;
	bool press, release;
};
REGISTER_POLYMORPHIC_CLASS(StateChange, ArkanoidState, "ArkanoidState");

ArkanoidPad::ArkanoidPad(MSXEventDistributor& eventDistributor_,
                         StateChangeDistributor& stateChangeDistributor_)
	: eventDistributor(eventDistributor_)
	, stateChangeDistributor(stateChangeDistributor_)
	, shiftreg(0) // the 9 bit shift degrades to 0
	, dialpos(POS_CENTER)
	, buttonStatus(0x3E)
	, lastValue(0)
{
}

ArkanoidPad::~ArkanoidPad()
{
	if (isPluggedIn()) {
		ArkanoidPad::unplugHelper(EmuTime::dummy());
	}
}


// Pluggable
const string& ArkanoidPad::getName() const
{
	static const string name("arkanoidpad");
	return name;
}

string_ref ArkanoidPad::getDescription() const
{
	return "Arkanoid pad";
}

void ArkanoidPad::plugHelper(Connector& /*connector*/, EmuTime::param /*time*/)
{
	eventDistributor.registerEventListener(*this);
	stateChangeDistributor.registerListener(*this);
}

void ArkanoidPad::unplugHelper(EmuTime::param /*time*/)
{
	stateChangeDistributor.unregisterListener(*this);
	eventDistributor.unregisterEventListener(*this);
}

// JoystickDevice
byte ArkanoidPad::read(EmuTime::param /*time*/)
{
	return buttonStatus | ((shiftreg & 0x100) >> 8);
}

void ArkanoidPad::write(byte value, EmuTime::param /*time*/)
{
	byte diff = lastValue ^ value;
	lastValue = value;

	if (diff & value & 0x4) {
		// pin 8 from low to high: copy dial position into shift reg
		shiftreg = dialpos;
	}
	if (diff & value & 0x1) {
		// pin 6 from low to high: shift the shift reg
		shiftreg = (shiftreg << 1) | (shiftreg & 1); // bit 0's value is 'shifted in'
	}
}

// MSXEventListener
void ArkanoidPad::signalEvent(const shared_ptr<const Event>& event,
                              EmuTime::param time)
{
	switch (event->getType()) {
	case OPENMSX_MOUSE_MOTION_EVENT: {
		auto& mEvent = checked_cast<const MouseMotionEvent&>(*event);
		int newPos = std::min(POS_MAX,
		                      std::max(POS_MIN,
		                               dialpos + mEvent.getX() / SCALE));
		int delta = newPos - dialpos;
		if (delta != 0) {
			stateChangeDistributor.distributeNew(
				make_shared<ArkanoidState>(
					time, delta, false, false));
		}
		break;
	}
	case OPENMSX_MOUSE_BUTTON_DOWN_EVENT:
		// any button will press the Arkanoid Pad button
		if (buttonStatus & 2) {
			stateChangeDistributor.distributeNew(
				make_shared<ArkanoidState>(
					time, 0, true, false));
		}
		break;
	case OPENMSX_MOUSE_BUTTON_UP_EVENT:
		// any button will unpress the Arkanoid Pad button
		if (!(buttonStatus & 2)) {
			stateChangeDistributor.distributeNew(
				make_shared<ArkanoidState>(
					time, 0, false, true));
		}
		break;
	default:
		// ignore
		break;
	}
}

// StateChangeListener
void ArkanoidPad::signalStateChange(const shared_ptr<StateChange>& event)
{
	auto as = dynamic_cast<ArkanoidState*>(event.get());
	if (!as) return;

	dialpos += as->getDelta();
	if (as->getPress())   buttonStatus &= ~2;
	if (as->getRelease()) buttonStatus |=  2;
}

void ArkanoidPad::stopReplay(EmuTime::param time)
{
	// TODO Get actual mouse button(s) state. Is it worth the trouble?
	int delta = POS_CENTER - dialpos;
	bool release = (buttonStatus & 2) == 0;
	if ((delta != 0) || release) {
		stateChangeDistributor.distributeNew(make_shared<ArkanoidState>(
			time, delta, false, release));
	}
}


// version 1: Initial version, the variables dialpos and buttonStatus were not
//            serialized.
// version 2: Also serialize the above variables, this is required for
//            record/replay, see comment in Keyboard.cc for more details.
template<typename Archive>
void ArkanoidPad::serialize(Archive& ar, unsigned version)
{
	ar.serialize("shiftreg", shiftreg);
	ar.serialize("lastValue", lastValue);
	if (ar.versionAtLeast(version, 2)) {
		ar.serialize("dialpos", dialpos);
		ar.serialize("buttonStatus", buttonStatus);
	}
	if (ar.isLoader() && isPluggedIn()) {
		plugHelper(*getConnector(), EmuTime::dummy());
	}
}
INSTANTIATE_SERIALIZE_METHODS(ArkanoidPad);
REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, ArkanoidPad, "ArkanoidPad");

} // namespace openmsx
