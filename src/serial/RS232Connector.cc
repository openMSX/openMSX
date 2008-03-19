// $Id$

#include "RS232Connector.hh"
#include "RS232Device.hh"
#include "DummyRS232Device.hh"
#include "PluggingController.hh"
#include "checked_cast.hh"

namespace openmsx {

RS232Connector::RS232Connector(PluggingController& pluggingController_,
                               const std::string &name)
	: Connector(name, std::auto_ptr<Pluggable>(new DummyRS232Device()))
	, pluggingController(pluggingController_)
{
	pluggingController.registerConnector(*this);
}

RS232Connector::~RS232Connector()
{
	pluggingController.unregisterConnector(*this);
}

const std::string& RS232Connector::getDescription() const
{
	static const std::string desc("Serial RS232 connector.");
	return desc;
}

const std::string& RS232Connector::getClass() const
{
	static const std::string className("RS232");
	return className;
}

RS232Device& RS232Connector::getPluggedRS232Dev() const
{
	return *checked_cast<RS232Device*>(&getPlugged());
}

} // namespace openmsx
