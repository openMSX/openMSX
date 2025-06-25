#include "MSXE6Timer.hh"
#include "serialize.hh"

namespace openmsx {

MSXE6Timer::MSXE6Timer(const DeviceConfig& config)
	: MSXDevice(config)
	, reference(getCurrentTime())
{
}

void MSXE6Timer::reset(EmuTime time)
{
	reference.reset(time);
}

void MSXE6Timer::writeIO(uint16_t /*port*/, byte /*value*/, EmuTime time)
{
	/*
	The Clock class rounds down time to its clock resolution.
	This is correct for the E6 timer, verified by running the following
	program in R800 mode:
		org &HC000
		di
		ld c,&HE6
	loop:
		out (&HE6),a
		in d,(c)
		jp z,loop
		ei
		ret
	It returns with D=1.
	*/
	reference.advance(time);
}

byte MSXE6Timer::readIO(uint16_t port, EmuTime time)
{
	return peekIO(port, time);
}

byte MSXE6Timer::peekIO(uint16_t port, EmuTime time) const
{
	auto counter = reference.getTicksTill(time);
	return (port & 1) ? ((counter >> 8) & 0xFF) : (counter & 0xFF);
}

template<typename Archive>
void MSXE6Timer::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	ar.serialize("reference", reference);
}
INSTANTIATE_SERIALIZE_METHODS(MSXE6Timer);
REGISTER_MSXDEVICE(MSXE6Timer, "E6Timer");

} // namespace openmsx
