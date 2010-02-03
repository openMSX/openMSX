// $Id$

#include "CassettePort.hh"
#include "CassetteDevice.hh"
#include "CassettePlayer.hh"
#include "components.hh"
#if COMPONENT_LASERDISC
#include "LaserdiscPlayer.hh"
#endif
#include "DummyCassetteDevice.hh"
#include "MSXMotherBoard.hh"
#include "GlobalSettings.hh"
#include "Reactor.hh"
#include "PluggingController.hh"
#include "checked_cast.hh"
#include "serialize.hh"
#include <memory>

using std::auto_ptr;
using std::string;

namespace openmsx {

// CassettePortInterface

CassettePortInterface::~CassettePortInterface()
{
}


// DummyCassettePort

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

CassettePort::CassettePort(MSXMotherBoard& motherBoard_)
	: Connector(motherBoard_.getPluggingController(), "cassetteport",
	            auto_ptr<Pluggable>(new DummyCassetteDevice()))
	, motherBoard(motherBoard_)
#if COMPONENT_LASERDISC
	, laserdiscPlayer(NULL)
#endif
	, lastOutput(false)
	, motorControl(false)
{
	cassettePlayer.reset(new CassettePlayer(
		motherBoard.getCommandController(),
		motherBoard.getMSXMixer(),
		motherBoard.getScheduler(),
		motherBoard.getStateChangeDistributor(),
		motherBoard.getReactor().getEventDistributor(),
		motherBoard.getMSXCliComm(),
		motherBoard.getReactor().getGlobalSettings().getResampleSetting(),
		motherBoard.getReactor().getGlobalSettings().getThrottleManager()
		));
	getPluggingController().registerPluggable(cassettePlayer.get());
}

CassettePort::~CassettePort()
{
	unplug(motherBoard.getCurrentTime());
	getPluggingController().unregisterPluggable(cassettePlayer.get());
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
	short sample;
#if COMPONENT_LASERDISC
	if (!motorControl && laserdiscPlayer != NULL) {
		sample = laserdiscPlayer->readSample(time);
	} else
#endif
	{
		sample = getPluggedCasDev().readSample(time); // read 1 sample
	}
	bool result = (sample >= 0); // comparator
	return result;
}

#if COMPONENT_LASERDISC
void CassettePort::setLaserdiscPlayer(LaserdiscPlayer *laserdiscPlayer_)
{
	laserdiscPlayer = laserdiscPlayer_;
}
#endif

void CassettePort::unplug(EmuTime::param time)
{
	Connector::unplug(time);
}

const string& CassettePort::getDescription() const
{
	static const string desc("MSX Cassette port.");
	return desc;
}

const string& CassettePort::getClass() const
{
	static const string className("Cassette Port");
	return className;
}

CassetteDevice& CassettePort::getPluggedCasDev() const
{
	return *checked_cast<CassetteDevice*>(&getPlugged());
}

template<typename Archive>
void CassettePort::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<Connector>(*this);
	// don't serialize 'lastOutput', done via MSXPPI
}
INSTANTIATE_SERIALIZE_METHODS(CassettePort);

} // namespace openmsx
