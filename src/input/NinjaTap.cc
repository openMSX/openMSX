#include "NinjaTap.hh"

#include "JoystickPort.hh"

#include "enumerate.hh"
#include "serialize.hh"

#include <algorithm>

// See this (long) MRC thread for a detailed discussion on NinjaTap, including
// an electric schema.
//   https://www.msx.org/forum/msx-talk/hardware/ninja-tap

namespace openmsx {

NinjaTap::NinjaTap(PluggingController& pluggingController_, std::string name_)
	: JoyTap(pluggingController_, std::move(name_))
{
	std::ranges::fill(buf, 0xFF);
}

zstring_view NinjaTap::getDescription() const
{
	return "MSX Ninja Tap device";
}


void NinjaTap::plugHelper(Connector& /*connector*/, EmuTime time)
{
	createPorts("Ninja Tap port", time);
}

uint8_t NinjaTap::read(EmuTime /*time*/)
{
	return status;
}

void NinjaTap::write(uint8_t value, EmuTime time)
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
				uint8_t t = slave->read(time);
				buf[i] = uint8_t(((t & 0x0F) << 4) |
				                 ((t & 0x30) >> 4) |
				                 0x0C);
			}
		}
		if (!(value & 4) && (previous & 4)) {
			// pin 8 1->0 :  shift values
			// TODO what about b4 and b5?
			uint8_t t = 0;
			for (auto [i, b] : enumerate(buf)) {
				if (b & 1) t |= (1 << i);
				b >>= 1;
			}
			status = (status & ~0x0F) | t;
		}
	}

	// pin7 (b5) is always the inverse of pin8
	// See Danjovic's post from 08-10-2022, 06:58
	//    https://www.msx.org/forum/msx-talk/hardware/ninja-tap?page=7
	if (value & 4) {
		status &= ~0x20;
	} else {
		status |= 0x20;
	}

	previous = value;
}


template<typename Archive>
void NinjaTap::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<JoyTap>(*this);
	ar.serialize("status",   status,
	             "previous", previous,
	             "buf",      buf);
}
INSTANTIATE_SERIALIZE_METHODS(NinjaTap);
REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, NinjaTap, "NinjaTap");

} // namespace openmsx
