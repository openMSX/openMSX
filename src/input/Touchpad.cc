// This implementation is based on the reverse-engineering effort done
// by 'SD-snatcher'. See here:
//   http://www.msx.org/news/en/msx-touchpad-protocol-reverse-engineered
//   http://www.msx.org/wiki/Touchpad
// Also thanks to Manuel Bilderbeek for donating a Philips NMS-1150 that
// made this effort possible.

#include "Touchpad.hh"
#include "MSXEventDistributor.hh"
#include "StateChangeDistributor.hh"
#include "Event.hh"
#include "StateChange.hh"
#include "Display.hh"
#include "OutputSurface.hh"
#include "CommandController.hh"
#include "CommandException.hh"
#include "Clock.hh"
#include "serialize.hh"
#include "serialize_meta.hh"
#include "xrange.hh"
#include <iostream>

using namespace gl;

namespace openmsx {

class TouchpadState final : public StateChange
{
public:
	TouchpadState() = default; // for serialize
	TouchpadState(EmuTime::param time_,
	              byte x_, byte y_, bool touch_, bool button_)
		: StateChange(time_)
		, x(x_), y(y_), touch(touch_), button(button_) {}
	[[nodiscard]] byte getX()      const { return x; }
	[[nodiscard]] byte getY()      const { return y; }
	[[nodiscard]] bool getTouch()  const { return touch; }
	[[nodiscard]] bool getButton() const { return button; }

	template<typename Archive> void serialize(Archive& ar, unsigned /*version*/)
	{
		ar.template serializeBase<StateChange>(*this);
		ar.serialize("x",      x,
		             "y",      y,
		             "touch",  touch,
		             "button", button);
	}
private:
	byte x, y;
	bool touch, button;
};
REGISTER_POLYMORPHIC_CLASS(StateChange, TouchpadState, "TouchpadState");


Touchpad::Touchpad(MSXEventDistributor& eventDistributor_,
                   StateChangeDistributor& stateChangeDistributor_,
                   Display& display_,
                   CommandController& commandController)
	: eventDistributor(eventDistributor_)
	, stateChangeDistributor(stateChangeDistributor_)
	, display(display_)
	, transformSetting(commandController,
		"touchpad_transform_matrix",
		"2x3 matrix to transform host mouse coordinates to "
		"MSX touchpad coordinates, see manual for details",
		"{ 256 0 0 } { 0 256 0 }")
	, start(EmuTime::zero())
	, hostButtons(0)
	, x(0), y(0), touch(false), button(false)
	, shift(0), channel(0), last(0)
{
	auto& interp = commandController.getInterpreter();
	transformSetting.setChecker([this, &interp](TclObject& newValue) {
		try {
			parseTransformMatrix(interp, newValue);
		} catch (CommandException& e) {
			throw CommandException(
				"Invalid transformation matrix: ", e.getMessage());
		}
	});
	try {
		parseTransformMatrix(interp, transformSetting.getValue());
	} catch (CommandException& e) {
		// should only happen when settings.xml was manually edited
		std::cerr << e.getMessage() << '\n';
		// fill in safe default values
		m[0][0] = 256.0f; m[1][0] =   0.0f; m[2][0] = 0.0f;
		m[0][1] =   0.0f; m[1][1] = 256.0f; m[2][1] = 0.0f;
	}
}

Touchpad::~Touchpad()
{
	if (isPluggedIn()) {
		Touchpad::unplugHelper(EmuTime::dummy());
	}
}

void Touchpad::parseTransformMatrix(Interpreter& interp, const TclObject& value)
{
	if (value.getListLength(interp) != 2) {
		throw CommandException("must have 2 rows");
	}
	for (auto i : xrange(2)) {
		TclObject row = value.getListIndex(interp, i);
		if (row.getListLength(interp) != 3) {
			throw CommandException("each row must have 3 elements");
		}
		for (auto j : xrange(3)) {
			m[j][i] = row.getListIndex(interp, j).getDouble(interp);
		}
	}
}

// Pluggable
std::string_view Touchpad::getName() const
{
	return "touchpad";
}

std::string_view Touchpad::getDescription() const
{
	return "MSX Touchpad";
}

void Touchpad::plugHelper(Connector& /*connector*/, EmuTime::param /*time*/)
{
	eventDistributor.registerEventListener(*this);
	stateChangeDistributor.registerListener(*this);
}

void Touchpad::unplugHelper(EmuTime::param /*time*/)
{
	stateChangeDistributor.unregisterListener(*this);
	eventDistributor.unregisterEventListener(*this);
}

// JoystickDevice
static constexpr byte SENSE  = JoystickDevice::RD_PIN1;
static constexpr byte EOC    = JoystickDevice::RD_PIN2;
static constexpr byte SO     = JoystickDevice::RD_PIN3;
static constexpr byte BUTTON = JoystickDevice::RD_PIN4;
static constexpr byte SCK    = JoystickDevice::WR_PIN6;
static constexpr byte SI     = JoystickDevice::WR_PIN7;
static constexpr byte CS     = JoystickDevice::WR_PIN8;

byte Touchpad::read(EmuTime::param time)
{
	byte result = SENSE | BUTTON; // 1-bit means not pressed
	if (touch)  result &= ~SENSE;
	if (button) result &= ~BUTTON;

	// EOC remains zero for 56 cycles after CS 0->1
	// TODO at what clock frequency does the UPD7001 run?
	//    400kHz is only the recommended value from the UPD7001 datasheet.
	constexpr EmuDuration delta = Clock<400000>::duration(56);
	if ((time - start) > delta) {
		result |= EOC;
	}

	if (shift & 0x80) result |= SO;
	if (last & CS)    result |= SO;

	return result | 0x30;
}

void Touchpad::write(byte value, EmuTime::param time)
{
	byte diff = last ^ value;
	last = value;
	if (diff & CS) {
		if (value & CS) {
			// CS 0->1
			channel = shift & 3;
			start = time; // to keep EOC=0 for 56 cycles
		} else {
			// CS 1->0
			// Tested by SD-Snatcher (see RFE #252):
			//  When not touched X is always 0, and Y floats
			//  between 147 and 149 (mostly 148).
			shift = (channel == 0) ? (touch ? x :   0)
			      : (channel == 3) ? (touch ? y : 148)
			      : 0; // channel 1 and 2 return 0
		}
	}
	if (((value & (CS | SCK)) == SCK) && (diff & SCK)) {
		// SC=0 & SCK 0->1
		shift <<= 1;
		shift |= (value & SI) != 0;
	}
}

ivec2 Touchpad::transformCoords(ivec2 xy)
{
	if (auto* output = display.getOutputSurface()) {
		vec2 uv = vec2(xy) / vec2(output->getLogicalSize());
		xy = ivec2(m * vec3(uv, 1.0f));
	}
	return clamp(xy, 0, 255);
}

// MSXEventListener
void Touchpad::signalMSXEvent(const Event& event,
                              EmuTime::param time) noexcept
{
	ivec2 pos = hostPos;
	int b = hostButtons;

	visit(overloaded{
		[&](const MouseMotionEvent& e) {
			pos = transformCoords(ivec2(e.getAbsX(), e.getAbsY()));
		},
		[&](const MouseButtonDownEvent& e) {
			switch (e.getButton()) {
			case MouseButtonEvent::LEFT:
				b |= 1;
				break;
			case MouseButtonEvent::RIGHT:
				b |= 2;
				break;
			default:
				// ignore other buttons
				break;
			}
		},
		[&](const MouseButtonUpEvent& e) {
			switch (e.getButton()) {
			case MouseButtonEvent::LEFT:
				b &= ~1;
				break;
			case MouseButtonEvent::RIGHT:
				b &= ~2;
				break;
			default:
				// ignore other buttons
				break;
			}
		},
		[](const EventBase&) { /*ignore*/ }
	}, event);

	if ((pos != hostPos) || (b != hostButtons)) {
		hostPos     = pos;
		hostButtons = b;
		createTouchpadStateChange(
			time, pos[0], pos[1],
			(hostButtons & 1) != 0,
			(hostButtons & 2) != 0);
	}
}

void Touchpad::createTouchpadStateChange(
	EmuTime::param time, byte x_, byte y_, bool touch_, bool button_)
{
	stateChangeDistributor.distributeNew<TouchpadState>(
		time, x_, y_, touch_, button_);
}

// StateChangeListener
void Touchpad::signalStateChange(const StateChange& event)
{
	if (const auto* ts = dynamic_cast<const TouchpadState*>(&event)) {
		x      = ts->getX();
		y      = ts->getY();
		touch  = ts->getTouch();
		button = ts->getButton();
	}
}

void Touchpad::stopReplay(EmuTime::param time) noexcept
{
	// TODO Get actual mouse state. Is it worth the trouble?
	if (x || y || touch || button) {
		stateChangeDistributor.distributeNew<TouchpadState>(
			time, 0, 0, false, false);
	}
}


template<typename Archive>
void Touchpad::serialize(Archive& ar, unsigned /*version*/)
{
	// no need to serialize hostX, hostY, hostButtons,
	//                      transformSetting, m[][]
	ar.serialize("start",   start,
	             "x",       x,
	             "y",       y,
	             "touch",   touch,
	             "button",  button,
	             "shift",   shift,
	             "channel", channel,
	             "last",    last);

	if constexpr (Archive::IS_LOADER) {
		if (isPluggedIn()) {
			plugHelper(*getConnector(), EmuTime::dummy());
		}
	}
}
INSTANTIATE_SERIALIZE_METHODS(Touchpad);
REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, Touchpad, "Touchpad");

} // namespace openmsx
