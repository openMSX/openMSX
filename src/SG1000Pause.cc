#include "SG1000Pause.hh"
#include "Event.hh"
#include "InputEvents.hh"
#include "Keys.hh"
#include "MSXCPU.hh"
#include "MSXEventDistributor.hh"
#include "MSXMotherBoard.hh"
#include "checked_cast.hh"
#include "serialize.hh"

namespace openmsx {

SG1000Pause::SG1000Pause(const DeviceConfig& config)
	: MSXDevice(config)
	, Schedulable(getMotherBoard().getScheduler())
{
	getMotherBoard().getMSXEventDistributor().registerEventListener(*this);
}

SG1000Pause::~SG1000Pause()
{
	getMotherBoard().getMSXEventDistributor().unregisterEventListener(*this);
}

void SG1000Pause::signalEvent(const std::shared_ptr<const Event>& event,
                              EmuTime::param time)
{
	EventType type = event->getType();
	if (type == OPENMSX_KEY_DOWN_EVENT) {
		auto& keyEvent = checked_cast<const KeyEvent&>(*event);
		if (keyEvent.getKeyCode() == Keys::K_F5) {
			setSyncPoint(time);
		}
	}
}

void SG1000Pause::executeUntil(EmuTime::param /*time*/)
{
	// We raise and then immediately lower the NMI request. This still triggers
	// an interrupt, since our CPU core remembers the edge.
	MSXCPU& cpu = getMotherBoard().getCPU();
	cpu.raiseNMI();
	cpu.lowerNMI();
}

template<typename Archive>
void SG1000Pause::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
}
INSTANTIATE_SERIALIZE_METHODS(SG1000Pause);
REGISTER_MSXDEVICE(SG1000Pause, "SG1000Pause");

} // namespace openmsx
