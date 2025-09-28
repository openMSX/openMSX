#include "MSXCielTurbo.hh"

#include "LedStatus.hh"
#include "MSXCPU.hh"
#include "serialize.hh"

namespace openmsx {

MSXCielTurbo::MSXCielTurbo(const DeviceConfig& config)
	: MSXDevice(config)
{
	reset(EmuTime::dummy());
}

void MSXCielTurbo::reset(EmuTime time)
{
	uint16_t port = 0; // dummy
	writeIO(port, 0, time);
}

byte MSXCielTurbo::readIO(uint16_t /*port*/, EmuTime /*time*/)
{
	return lastValue;
}

byte MSXCielTurbo::peekIO(uint16_t /*port*/, EmuTime /*time*/) const
{
	return lastValue;
}

void MSXCielTurbo::writeIO(uint16_t /*port*/, byte value, EmuTime /*time*/)
{
	lastValue = value;
	bool enabled = (value & 0x80) != 0;
	unsigned freq = 3579545;
	if (enabled) freq *= 2;
	getCPU().setZ80Freq(freq);
	getLedStatus().setLed(LedStatus::TURBO, enabled);
}

template<typename Archive>
void MSXCielTurbo::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);

	ar.serialize("value", lastValue);
	if constexpr (!Archive::IS_LOADER) {
		uint16_t port = 0; // dummy
		writeIO(port, lastValue, EmuTime::dummy());
	}
}
INSTANTIATE_SERIALIZE_METHODS(MSXCielTurbo);
REGISTER_MSXDEVICE(MSXCielTurbo, "CielTurbo");

} // namespace openmsx
