// $Id$

#include "RS232Connector.hh"
#include "RS232Device.hh"
#include "DummyRS232Device.hh"
#include "RS232Tester.hh"
#include "PluggingController.hh"


RS232Connector::RS232Connector(const string& name_, const EmuTime& time)
	: name(name_)
{
	dummy = new DummyRS232Device();
	tester = new RS232Tester();
	PluggingController::instance()->registerConnector(this);

	unplug(time);
}

RS232Connector::~RS232Connector()
{
	PluggingController::instance()->unregisterConnector(this);
	delete tester;
	delete dummy;
}

const string& RS232Connector::getName() const
{
	return name;
}

const string& RS232Connector::getClass() const
{
	static const string className("RS232");
	return className;
}

void RS232Connector::plug(Pluggable* device, const EmuTime &time)
{
	Connector::plug(device, time);
}

void RS232Connector::unplug(const EmuTime& time)
{
	Connector::unplug(time);
	plug(dummy, time);
}
