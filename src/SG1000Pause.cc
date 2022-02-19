#include "SG1000Pause.hh"
#include "Event.hh"
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

void SG1000Pause::signalMSXEvent(const Event& event, EmuTime::param time) noexcept
{
	visit(overloaded{
		[&](const KeyDownEvent& keyEvent) {
			if (keyEvent.getKeyCode() == Keys::K_F5) {
				setSyncPoint(time);
			}
		},
		[](const EventBase&) { /*ignore*/ }
	}, event);
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
