// $Id$

#include "Y8950KeyboardConnector.hh"
#include "PluggingController.hh"


namespace openmsx {

Y8950KeyboardConnector::Y8950KeyboardConnector()
	: Connector("audiokeyboardport", new DummyY8950KeyboardDevice())
	, data(255)
{
	PluggingController::instance()->registerConnector(this);
}

Y8950KeyboardConnector::~Y8950KeyboardConnector()
{
	PluggingController::instance()->unregisterConnector(this);
}

void Y8950KeyboardConnector::write(byte newData, const EmuTime &time)
{
	if (newData != data) {
		data = newData;
		((Y8950KeyboardDevice*)pluggable)->write(data, time);
	}
}

byte Y8950KeyboardConnector::read(const EmuTime &time)
{
	return ((Y8950KeyboardDevice*)pluggable)->read(time);
}

const string& Y8950KeyboardConnector::getDescription() const
{
	static const string desc("MSX-AUDIO keyboard connector.");
	return desc;
}

const string& Y8950KeyboardConnector::getClass() const
{
	static const string className("Y8950 Keyboard Port");
	return className;
}

void Y8950KeyboardConnector::plug(Pluggable *dev, const EmuTime &time)
	throw(PlugException)
{
	Connector::plug(dev, time);
	((Y8950KeyboardDevice*)pluggable)->write(data, time);
}


// --- DummyY8950KeyboardDevice ---

void DummyY8950KeyboardDevice::write(byte data, const EmuTime &time)
{
	// ignore data
}

byte DummyY8950KeyboardDevice::read(const EmuTime &time)
{
	return 255;
}

const string& DummyY8950KeyboardDevice::getDescription() const
{
	static const string EMPTY;
	return EMPTY;
}

void DummyY8950KeyboardDevice::plug(Connector* connector, const EmuTime& time)
	throw()
{
}

void DummyY8950KeyboardDevice::unplug(const EmuTime& time)
{
}

} // namespace openmsx
