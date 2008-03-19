// $Id$

#include "Y8950KeyboardConnector.hh"
#include "Y8950KeyboardDevice.hh"
#include "DummyY8950KeyboardDevice.hh"
#include "PluggingController.hh"
#include "checked_cast.hh"

namespace openmsx {

Y8950KeyboardConnector::Y8950KeyboardConnector(
	PluggingController& pluggingController_)
	: Connector("audiokeyboardport",
	            std::auto_ptr<Pluggable>(new DummyY8950KeyboardDevice()))
	, pluggingController(pluggingController_)
	, data(255)
{
	pluggingController.registerConnector(*this);
}

Y8950KeyboardConnector::~Y8950KeyboardConnector()
{
	pluggingController.unregisterConnector(*this);
}

void Y8950KeyboardConnector::write(byte newData, const EmuTime& time)
{
	if (newData != data) {
		data = newData;
		getPluggedKeyb().write(data, time);
	}
}

byte Y8950KeyboardConnector::read(const EmuTime& time)
{
	return getPluggedKeyb().read(time);
}

const std::string& Y8950KeyboardConnector::getDescription() const
{
	static const std::string desc("MSX-AUDIO keyboard connector.");
	return desc;
}

const std::string& Y8950KeyboardConnector::getClass() const
{
	static const std::string className("Y8950 Keyboard Port");
	return className;
}

void Y8950KeyboardConnector::plug(Pluggable& dev, const EmuTime& time)
{
	Connector::plug(dev, time);
	getPluggedKeyb().write(data, time);
}

Y8950KeyboardDevice& Y8950KeyboardConnector::getPluggedKeyb() const
{
	return *checked_cast<Y8950KeyboardDevice*>(&getPlugged());
}

} // namespace openmsx
