// $Id$

#include "NinjaTap.hh"
#include "JoystickPort.hh"

namespace openmsx {

NinjaTap::NinjaTap(PluggingController& pluggingController,
                   const std::string& name)
	: JoyTap(pluggingController, name)
{
	status = 0x3F; // TODO check initial value
	previous = 0;
	for (int i = 0; i < 4; ++i) {
		buf[i] = 0xFF;
	}
}

const std::string& NinjaTap::getDescription() const
{
	static const std::string desc("MSX NinjaTap device.");
	return desc;
}

byte NinjaTap::read(const EmuTime& /*time*/)
{
	return status;
}

void NinjaTap::write(byte value, const EmuTime& time)
{
	// bit 0 -> pin 6
	// bit 1 -> pin 7
	// bit 2 -> pin 8
	if (value & 2) {
		// pin7 = 1 :  read mode
		if (!(value & 1) && (previous & 1)) {
			// pin 6 1->0 :  query joysticks
			// TODO does output change?
			for (int i = 0; i < 4; ++i) {
				byte t = slaves[i]->read(time);
				buf[i] = ((t & 0x0F) << 4) |
				         ((t & 0x30) >> 4) |
				         0x0C;
			}
		}
		if (!(value & 4) && (previous & 4)) {
			// pin 8 1->0 :  shift values
			// TODO what about b4 and b5?
			byte t = 0;
			for (int i = 0; i < 4; ++i) {
				if (buf[i] & 1) t |= (1 << i);
				buf[i] >>= 1;
			}
			status = (status & ~0x0F) | t;
		}
	} else {
		// pin 7 = 0 :  detect mode, b5 is inverse of pin8
		// TODO what happens with other bits?
		if (value & 4) {
			status &= ~0x20;
		} else {
			status |= 0x20;
		}
	}
	previous = value;
}

} // namespace openmsx
