#include "MSXHiResTimer.hh"
#include "narrow.hh"
#include "serialize.hh"

namespace openmsx {

MSXHiResTimer::MSXHiResTimer(const DeviceConfig& config)
	: MSXDevice(config)
	, reference(getCurrentTime())
{
	reset(getCurrentTime());
}

void MSXHiResTimer::reset(EmuTime::param time)
{
	reference.reset(time);
	latchedValue = 0;
}

void MSXHiResTimer::writeIO(uint16_t /*port*/, byte /*value*/, EmuTime::param time)
{
	// write to any port will reset the counter
	reference.advance(time);
}

byte MSXHiResTimer::readIO(uint16_t port, EmuTime::param time)
{
	if ((port & 3) == 0) {
		// reading port 0 will latch the current counter value
		latchedValue = reference.getTicksTill(time);
	}
	// reading port {0, 1, 2, 3} will read bits {0-7, 8-15, 16-23, 24-32}
	// of the counter
	return narrow_cast<byte>(latchedValue >> (8 * (port & 3)));
}

byte MSXHiResTimer::peekIO(uint16_t port, EmuTime::param time) const
{
	unsigned tmp = (port & 3) ? latchedValue : reference.getTicksTill(time);
	return narrow_cast<byte>(tmp >> (8 * (port & 3)));
}

template<typename Archive>
void MSXHiResTimer::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	ar.serialize("reference",    reference,
	             "latchedValue", latchedValue);
}
INSTANTIATE_SERIALIZE_METHODS(MSXHiResTimer);
REGISTER_MSXDEVICE(MSXHiResTimer, "HiResTimer");

} // namespace openmsx
