#include "CassettePort.hh"
#include "CassetteDevice.hh"
#include "CassettePlayer.hh"
#include "components.hh"
#if COMPONENT_LASERDISC
#include "LaserdiscPlayer.hh"
#endif
#include "DummyCassetteDevice.hh"
#include "HardwareConfig.hh"
#include "MSXMotherBoard.hh"
#include "PluggingController.hh"
#include "checked_cast.hh"
#include "serialize.hh"
#include <memory>

namespace openmsx {

// DummyCassettePort

DummyCassettePort::DummyCassettePort(MSXMotherBoard& motherBoard)
	: cassettePlayerCommand(
		nullptr,
		motherBoard.getCommandController(),
		motherBoard.getStateChangeDistributor(),
		motherBoard.getScheduler())
{
}

void DummyCassettePort::setMotor(bool /*status*/, EmuTime::param /*time*/)
{
	// do nothing
}
void DummyCassettePort::cassetteOut(bool /*output*/, EmuTime::param /*time*/)
{
	// do nothing
}
bool DummyCassettePort::cassetteIn(EmuTime::param /*time*/)
{
	return false;
}
bool DummyCassettePort::lastOut() const
{
	return false; // not relevant
}
#if COMPONENT_LASERDISC
void DummyCassettePort::setLaserdiscPlayer(LaserdiscPlayer* /* laserdisc */)
{
	// do nothing
}
#endif


// CassettePort

CassettePort::CassettePort(const HardwareConfig& hwConf)
	: Connector(hwConf.getMotherBoard().getPluggingController(), "cassetteport",
	            std::make_unique<DummyCassetteDevice>())
	, motherBoard(hwConf.getMotherBoard())
{
	auto player = std::make_unique<CassettePlayer>(hwConf);
	cassettePlayer = player.get();
	getPluggingController().registerPluggable(std::move(player));
}

CassettePort::~CassettePort()
{
	unplug(motherBoard.getCurrentTime());
}


void CassettePort::setMotor(bool status, EmuTime::param time)
{
	// TODO make 'click' sound
	motorControl = status;
	getPluggedCasDev().setMotor(status, time);
}

void CassettePort::cassetteOut(bool output, EmuTime::param time)
{
	lastOutput = output;
	// leave everything to the pluggable
	getPluggedCasDev().setSignal(output, time);
}

bool CassettePort::lastOut() const
{
	return lastOutput;
}

bool CassettePort::cassetteIn(EmuTime::param time)
{
	// All analog filtering is ignored for now
	//   only important component is DC-removal
	//   we just assume sample has no DC component
	int16_t sample = [&]{
	#if COMPONENT_LASERDISC
		if (!motorControl && laserdiscPlayer) {
			return laserdiscPlayer->readSample(time);
		}
	#endif
		return getPluggedCasDev().readSample(time); // read 1 sample
	}();
	return sample >= 0; // comparator
}

#if COMPONENT_LASERDISC
void CassettePort::setLaserdiscPlayer(LaserdiscPlayer *laserdiscPlayer_)
{
	laserdiscPlayer = laserdiscPlayer_;
}
#endif

std::string_view CassettePort::getDescription() const
{
	return "MSX Cassette port";
}

std::string_view CassettePort::getClass() const
{
	return "Cassette Port";
}

CassetteDevice& CassettePort::getPluggedCasDev() const
{
	return *checked_cast<CassetteDevice*>(&getPlugged());
}

template<typename Archive>
void CassettePort::serialize(Archive& ar, unsigned version)
{
	ar.template serializeBase<Connector>(*this);
	// don't serialize 'lastOutput', done via MSXPPI

	// Must come after serialization of the connector because that one
	// potentially serializes the CassettePlayer.
	if (ar.versionAtLeast(version, 2)) {
		// always serialize CassettePlayer, even if it's not plugged in.
		ar.serializeOnlyOnce("cassettePlayer", *cassettePlayer);
	}
}
INSTANTIATE_SERIALIZE_METHODS(CassettePort);

} // namespace openmsx
