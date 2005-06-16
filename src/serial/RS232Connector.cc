// $Id$

#include "RS232Connector.hh"
#include "DummyRS232Device.hh"
#include "PluggingController.hh"

using std::string;

namespace openmsx {

RS232Connector::RS232Connector(PluggingController& pluggingController_,
                               const string &name)
	: Connector(name, std::auto_ptr<Pluggable>(new DummyRS232Device()))
	, pluggingController(pluggingController_)
{
	pluggingController.registerConnector(this);
}

RS232Connector::~RS232Connector()
{
	pluggingController.unregisterConnector(this);
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

RS232Device& RS232Connector::getPlugged() const
{
	return static_cast<RS232Device&>(*plugged);
}

} // namespace openmsx
