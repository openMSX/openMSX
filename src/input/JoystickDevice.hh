#ifndef JOYSTICKDEVICE_HH
#define JOYSTICKDEVICE_HH

#include "Pluggable.hh"
#include "openmsx.hh"

namespace openmsx {

class JoystickDevice : public Pluggable
{
public:
	/**
	 * Read from the joystick device. The bits in the read byte have
	 * following meaning:
	 *   7    6       5         4         3       2      1     0
	 * | xx | xx | BUTTON_B | BUTTON_A | RIGHT | LEFT | DOWN | UP  |
	 * | xx | xx | pin7     | pin6     | pin4  | pin3 | pin2 | pin1|
	 */
	[[nodiscard]] virtual byte read(EmuTime::param time) = 0;

	/**
	 * Write a value to the joystick device. The bits in the written
	 * byte have following meaning:
	 *     7    6    5    4    3     2      1      0
	 *   | xx | xx | xx | xx | xx | pin8 | pin7 | pin6 |
	 * As an optimization, this method might not be called when the
	 * new value is the same as the previous one.
	 */
	virtual void write(byte value, EmuTime::param time) = 0;

	[[nodiscard]] std::string_view getClass() const final;

	/* Missing pin descriptions
	 * pin 5 : +5V
	 * pin 9 : GND
	 */

	// use in the read() method
	static constexpr int JOY_UP      = 0x01;
	static constexpr int JOY_DOWN    = 0x02;
	static constexpr int JOY_LEFT    = 0x04;
	static constexpr int JOY_RIGHT   = 0x08;
	static constexpr int JOY_BUTTONA = 0x10;
	static constexpr int JOY_BUTTONB = 0x20;
	static constexpr int RD_PIN1 = 0x01;
	static constexpr int RD_PIN2 = 0x02;
	static constexpr int RD_PIN3 = 0x04;
	static constexpr int RD_PIN4 = 0x08;
	static constexpr int RD_PIN6 = 0x10;
	static constexpr int RD_PIN7 = 0x20;

	// use in the write() method
	static constexpr int WR_PIN6 = 0x01;
	static constexpr int WR_PIN7 = 0x02;
	static constexpr int WR_PIN8 = 0x04;
};

} // namespace openmsx

#endif
