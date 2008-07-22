// $Id$

#include "CassettePort.hh"
#include "CassetteDevice.hh"
#include "CassettePlayer.hh"
#include "components.hh"
#ifdef COMPONENT_JACK
#include "CassetteJack.hh"
#endif
#include "DummyCassetteDevice.hh"
#include "MSXMotherBoard.hh"
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
	return false;
}
bool DummyCassettePort::lastOut() const
{
	return false; // not relevant
}


// CassettePort

CassettePort::CassettePort(MSXMotherBoard& motherBoard_)
	: Connector(motherBoard_.getPluggingController(), "cassetteport",
	            auto_ptr<Pluggable>(new DummyCassetteDevice()))
	, motherBoard(motherBoard_)
	, lastOutput(false)
{
	cassettePlayer.reset(new CassettePlayer(
		motherBoard.getCommandController(),
		motherBoard.getMSXMixer(),
		motherBoard.getScheduler(),
		motherBoard.getMSXEventDistributor(),
		motherBoard.getEventDistributor(),
		motherBoard.getMSXCliComm()));
	getPluggingController().registerPluggable(cassettePlayer.get());
#ifdef COMPONENT_JACK
	cassetteJack.reset(new CassetteJack(motherBoard.getScheduler()));
	getPluggingController().registerPluggable(cassetteJack.get());
#endif
}

CassettePort::~CassettePort()
{
	unplug(motherBoard.getCurrentTime());
	getPluggingController().unregisterPluggable(cassettePlayer.get());
#ifdef COMPONENT_JACK
	getPluggingController().unregisterPluggable(cassetteJack.get());
#endif
}


void CassettePort::setMotor(bool status, const EmuTime& time)
{
	//TODO make 'click' sound
	//PRT_DEBUG("CassettePort: motor " << status);
	getPluggedCasDev().setMotor(status, time);
}

void CassettePort::cassetteOut(bool output, const EmuTime& time)
{
	lastOutput = output;
	// leave everything to the pluggable
	getPluggedCasDev().setSignal(output, time);
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
	short sample = getPluggedCasDev().readSample(time); // read 1 sample
	bool result = (sample >= 0); // comparator
	//PRT_DEBUG("CassettePort:: read " << result);
	return result;
}

void CassettePort::unplug(const EmuTime& time)
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
