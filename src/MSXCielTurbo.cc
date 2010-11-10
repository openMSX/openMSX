// $Id$

#include "MSXCielTurbo.hh"
#include "MSXCPU.hh"
#include "MSXMotherBoard.hh"
#include "serialize.hh"

namespace openmsx {

static const byte ID = 0x08;

MSXCielTurbo::MSXCielTurbo(MSXMotherBoard& motherBoard,
                           const XMLElement& config)
	: MSXDevice(motherBoard, config)
{
	reset(EmuTime::dummy());
}

MSXCielTurbo::~MSXCielTurbo()
{
}

void MSXCielTurbo::reset(EmuTime::param time)
{
	word port = 0; // dummy
	writeIO(port, 0, time);
}

byte MSXCielTurbo::readIO(word /*port*/, EmuTime::param /*time*/)
{
	return lastValue;
}

byte MSXCielTurbo::peekIO(word /*port*/, EmuTime::param /*time*/) const
{
	return lastValue;
}

void MSXCielTurbo::writeIO(word /*port*/, byte value, EmuTime::param /*time*/)
{
	lastValue = value;
	unsigned freq = 3579545;
	if (value & 0x80) freq *= 2;
	getMotherBoard().getCPU().setZ80Freq(freq);
}

template<typename Archive>
void MSXCielTurbo::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);

	ar.serialize("value", lastValue);
	if (!ar.isLoader()) {
		word port = 0; // dummy
		writeIO(port, lastValue, EmuTime::dummy());
	}
}
INSTANTIATE_SERIALIZE_METHODS(MSXCielTurbo);
REGISTER_MSXDEVICE(MSXCielTurbo, "CielTurbo");

} // namespace openmsx
