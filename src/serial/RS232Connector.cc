// $Id$

#include "RS232Connector.hh"
#include "RS232Device.hh"
#include "DummyRS232Device.hh"
#include "RS232Tester.hh"
#include "PluggingController.hh"


namespace openmsx {

RS232Connector::RS232Connector(const string &name)
	: Connector(name, new DummyRS232Device())
{
	tester = new RS232Tester();
	PluggingController::instance()->registerConnector(this);
}

RS232Connector::~RS232Connector()
{
	PluggingController::instance()->unregisterConnector(this);
	delete tester;
}

const string &RS232Connector::getClass() const
{
	static const string className("RS232");
	return className;
}

} // namespace openmsx
