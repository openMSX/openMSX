#ifndef TOUCHPAD_HH
#define TOUCHPAD_HH

#include "EmuTime.hh"
#include "JoystickDevice.hh"
#include "MSXEventListener.hh"
#include "StateChangeListener.hh"
#include "StringSetting.hh"
#include "gl_mat.hh"

namespace openmsx {

class MSXEventDistributor;
class StateChangeDistributor;
class Display;
class CommandController;
class TclObject;
class Interpreter;

class Touchpad final : public JoystickDevice, private MSXEventListener
                     , private StateChangeListener
{
public:
	Touchpad(MSXEventDistributor& eventDistributor,
	         StateChangeDistributor& stateChangeDistributor,
	         Display& display,
	         CommandController& commandController);
	~Touchpad() override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void createTouchpadStateChange(EmuTime::param time,
		byte x, byte y, bool touch, bool button);
	void parseTransformMatrix(Interpreter& interp, const TclObject& value);
	[[nodiscard]] gl::ivec2 transformCoords(gl::ivec2 xy);

	// Pluggable
	[[nodiscard]] std::string_view getName() const override;
	[[nodiscard]] std::string_view getDescription() const override;
	void plugHelper(Connector& connector, EmuTime::param time) override;
	void unplugHelper(EmuTime::param time) override;

	// JoystickDevice
	[[nodiscard]] byte read(EmuTime::param time) override;
	void write(byte value, EmuTime::param time) override;

	// MSXEventListener
	void signalMSXEvent(const Event& event,
	                    EmuTime::param time) noexcept override;
	// StateChangeListener
	void signalStateChange(const StateChange& event) override;
	void stopReplay(EmuTime::param time) noexcept override;

private:
	MSXEventDistributor& eventDistributor;
	StateChangeDistributor& stateChangeDistributor;
	Display& display;

	StringSetting transformSetting;
	gl::matMxN<2, 3, float> m; // transformation matrix

	EmuTime start = EmuTime::zero(); // last time when CS switched 0->1
	gl::ivec2 hostPos;    // host state
	byte hostButtons = 0; //
	byte x = 0, y = 0;                  // msx state (different from host state
	bool touch = false, button = false; //            during replay)
	byte shift = 0; // shift register to both transmit and receive data
	byte channel = 0; // [0..3]   0->x, 3->y, 1,2->not used
	byte last = 0; // last written data, to detect transitions
};

} // namespace openmsx

#endif
