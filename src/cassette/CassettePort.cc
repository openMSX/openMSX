// $Id$

#include "CassettePort.hh"
#include "CassettePlayer.hh"
#include "components.hh"
#ifdef COMPONENT_JACK
#include "CassetteJack.hh"
#endif
#include "DummyCassetteDevice.hh"
#include "MSXMotherBoard.hh"
#include "PluggingController.hh"
#include <memory>

using std::auto_ptr;
using std::string;

namespace openmsx {

// CassettePortInterface //

CassettePortInterface::CassettePortInterface()
    : Connector("cassetteport", auto_ptr<Pluggable>(new DummyCassetteDevice()))
{
}

void CassettePortInterface::unplug(const EmuTime& time)
{
	Connector::unplug(time);
}

const string& CassettePortInterface::getDescription() const
{
	static const string desc("MSX Cassette port.");
	return desc;
}

const string& CassettePortInterface::getClass() const
{
	static const string className("Cassette Port");
	return className;
}

CassetteDevice& CassettePortInterface::getPlugged() const
{
	return static_cast<CassetteDevice&>(*plugged);
}


// DummyCassettePort //

DummyCassettePort::DummyCassettePort()
	: CassettePortInterface()
{
}

void DummyCassettePort::setMotor(bool /*status*/, const EmuTime& /*time*/)
{
	// do nothing
}
void DummyCassettePort::cassetteOut(bool /*output*/, const EmuTime& /*time*/)
{
	// do nothing
}
bool DummyCassettePort::cassetteIn(const EmuTime& /*time*/)
{
	return true;	// TODO check on Turbo-R
}
bool DummyCassettePort::lastOut() const
{
  return false; // not relevant
}


// CassettePort //

CassettePort::CassettePort(MSXMotherBoard& motherBoard)
	: CassettePortInterface()
	, pluggingController(motherBoard.getPluggingController())
        , nextSample(0)
	, prevTime(EmuTime::zero)
{
	cassettePlayer.reset(new CassettePlayer(
		motherBoard.getCommandController(),
		motherBoard.getMixer(),
		motherBoard.getScheduler()));
	pluggingController.registerConnector(*this);
	pluggingController.registerPluggable(cassettePlayer.get());
#ifdef COMPONENT_JACK
	cassetteJack.reset(new CassetteJack(motherBoard.getScheduler()));
	pluggingController.registerPluggable(cassetteJack.get());
#endif
}

CassettePort::~CassettePort()
{
	unplug(prevTime);
	pluggingController.unregisterPluggable(cassettePlayer.get());
#ifdef COMPONENT_JACK
	pluggingController.unregisterPluggable(cassetteJack.get());
#endif
	pluggingController.unregisterConnector(*this);
}


void CassettePort::setMotor(bool status, const EmuTime& time)
{
	//TODO make 'click' sound
	//PRT_DEBUG("CassettePort: motor " << status);
	getPlugged().setMotor(status, time);
	prevTime = time;
}

void CassettePort::cassetteOut(bool output, const EmuTime& time)
{
	lastOutput = output;
	// leave everything to the pluggable
	getPlugged().setSignal(output, time);
	prevTime = time;
}

bool CassettePort::lastOut() const
{
	return lastOutput;
}

bool CassettePort::cassetteIn(const EmuTime& time)
{
	// All analog filtering is ignored for now
	//   only important component is DC-removal
	//   we just assume sample has no DC component
	short sample = getPlugged().readSample(time); // read 1 sample
	bool result = (sample >= 0); // comparator
	//PRT_DEBUG("CassettePort:: read " << result);
	return result;
}

} // namespace openmsx
