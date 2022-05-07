#include "NinjaTap.hh"
#include "JoystickPort.hh"
#include "enumerate.hh"
#include "ranges.hh"
#include "serialize.hh"

namespace openmsx {

NinjaTap::NinjaTap(PluggingController& pluggingController_, std::string name_)
	: JoyTap(pluggingController_, std::move(name_))
	, status(0x3F) // TODO check initial value
	, previous(0)
{
	ranges::fill(buf, 0xFF);
}

std::string_view NinjaTap::getDescription() const
{
	return "MSX Ninja Tap device";
}


void NinjaTap::plugHelper(Connector& /*connector*/, EmuTime::param /*time*/)
{
	createPorts("Ninja Tap port");
}

byte NinjaTap::read(EmuTime::param /*time*/)
{
	return status;
}

void NinjaTap::write(byte value, EmuTime::param time)
{
	// bit 0 -> pin 6
	// bit 1 -> pin 7
	// bit 2 -> pin 8
	if (value & 2) {
		// pin7 = 1 :  read mode
		if (!(value & 1) && (previous & 1)) {
			// pin 6 1->0 :  query joysticks
			// TODO does output change?
			for (auto [i, slave] : enumerate(slaves)) {
				byte t = slave->read(time);
				buf[i] = ((t & 0x0F) << 4) |
				         ((t & 0x30) >> 4) |
				         0x0C;
			}
		}
		if (!(value & 4) && (previous & 4)) {
			// pin 8 1->0 :  shift values
			// TODO what about b4 and b5?
			byte t = 0;
			for (auto [i, b] : enumerate(buf)) {
				if (b & 1) t |= (1 << i);
				b >>= 1;
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


void NinjaTap::serialize(Archive auto& ar, unsigned /*version*/)
{
	ar.template serializeBase<JoyTap>(*this);
	ar.serialize("status",   status,
	             "previous", previous,
	             "buf",      buf);
}
INSTANTIATE_SERIALIZE_METHODS(NinjaTap);
REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, NinjaTap, "NinjaTap");

} // namespace openmsx
