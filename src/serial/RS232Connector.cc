// $Id$

#include "RS232Connector.hh"
#include "RS232Device.hh"
#include "DummyRS232Device.hh"
#include "PluggingController.hh"


namespace openmsx {

RS232Connector::RS232Connector(const string &name)
	: Connector(name, new DummyRS232Device())
{
	PluggingController::instance()->registerConnector(this);
}

RS232Connector::~RS232Connector()
{
	PluggingController::instance()->unregisterConnector(this);
}

const string& RS232Connector::getDescription() const
{
	static const string desc("Serial RS232 connector.");
	return desc;
}

const string& RS232Connector::getClass() const
{
	static const string className("RS232");
	return className;
}

} // namespace openmsx
