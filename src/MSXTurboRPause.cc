#include "MSXTurboRPause.hh"
#include "LedStatus.hh"
#include "MSXMotherBoard.hh"
#include "serialize.hh"

namespace openmsx {

MSXTurboRPause::MSXTurboRPause(const DeviceConfig& config)
	: MSXDevice(config)
	, pauseSetting(
		getCommandController(), "turborpause",
		"status of the TurboR pause", false)
{
	pauseSetting.attach(*this);
	reset(EmuTime::dummy());
}

MSXTurboRPause::~MSXTurboRPause()
{
	powerDown(EmuTime::dummy());
	pauseSetting.detach(*this);
}

void MSXTurboRPause::powerDown(EmuTime time)
{
	writeIO(0, 0, time); // send LED OFF events (if needed)
}

void MSXTurboRPause::reset(EmuTime time)
{
	pauseSetting.setBoolean(false);
	writeIO(0, 0, time);
}

byte MSXTurboRPause::readIO(uint16_t port, EmuTime time)
{
	return peekIO(port, time);
}

byte MSXTurboRPause::peekIO(uint16_t /*port*/, EmuTime /*time*/) const
{
	return pauseSetting.getBoolean() ? 1 : 0;
}

void MSXTurboRPause::writeIO(uint16_t /*port*/, byte value, EmuTime /*time*/)
{
	status = value;
	if (bool newTurboLed = (status & 0x80) != 0;
	    newTurboLed != turboLed) {
		turboLed = newTurboLed;
		getLedStatus().setLed(LedStatus::TURBO, turboLed);
	}
	updatePause();
}

void MSXTurboRPause::update(const Setting& /*setting*/) noexcept
{
	updatePause();
}

void MSXTurboRPause::updatePause()
{
	if (bool newHwPause = (status & 0x02) && pauseSetting.getBoolean();
	    newHwPause != hwPause) {
		hwPause = newHwPause;
		if (hwPause) {
			getMotherBoard().pause();
		} else {
			getMotherBoard().unpause();
		}
	}

	bool newPauseLed = (status & 0x01) || hwPause;
	if (newPauseLed != pauseLed) {
		pauseLed = newPauseLed;
		getLedStatus().setLed(LedStatus::PAUSE, pauseLed);
	}
}

template<typename Archive>
void MSXTurboRPause::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	ar.serialize("status", status);
	if constexpr (Archive::IS_LOADER) {
		writeIO(0, status, EmuTime::dummy());
	}
}
INSTANTIATE_SERIALIZE_METHODS(MSXTurboRPause);
REGISTER_MSXDEVICE(MSXTurboRPause, "TurboRPause");

} // namespace openmsx
