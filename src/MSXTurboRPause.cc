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
	, status(255)
	, pauseLed(false)
	, turboLed(false)
	, hwPause(false)
{
	pauseSetting.attach(*this);
	reset(EmuTime::dummy());
}

MSXTurboRPause::~MSXTurboRPause()
{
	powerDown(EmuTime::dummy());
	pauseSetting.detach(*this);
}

void MSXTurboRPause::powerDown(EmuTime::param time)
{
	writeIO(0, 0, time); // send LED OFF events (if needed)
}

void MSXTurboRPause::reset(EmuTime::param time)
{
	pauseSetting.setBoolean(false);
	writeIO(0, 0, time);
}

byte MSXTurboRPause::readIO(word port, EmuTime::param time)
{
	return peekIO(port, time);
}

byte MSXTurboRPause::peekIO(word /*port*/, EmuTime::param /*time*/) const
{
	return pauseSetting.getBoolean() ? 1 : 0;
}

void MSXTurboRPause::writeIO(word /*port*/, byte value, EmuTime::param /*time*/)
{
	status = value;
	bool newTurboLed = (status & 0x80) != 0;
	if (newTurboLed != turboLed) {
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
	bool newHwPause = (status & 0x02) && pauseSetting.getBoolean();
	if (newHwPause != hwPause) {
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

template<Archive Ar>
void MSXTurboRPause::serialize(Ar& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	ar.serialize("status", status);
	if constexpr (Ar::IS_LOADER) {
		writeIO(0, status, EmuTime::dummy());
	}
}
INSTANTIATE_SERIALIZE_METHODS(MSXTurboRPause);
REGISTER_MSXDEVICE(MSXTurboRPause, "TurboRPause");

} // namespace openmsx
