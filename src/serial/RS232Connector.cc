#include "RS232Connector.hh"

#include "DummyRS232Device.hh"
#include "RS232Device.hh"

#include "serialize.hh"

#include "checked_cast.hh"

#include <memory>

namespace openmsx {

RS232Connector::RS232Connector(PluggingController& pluggingController_,
                               std::string name_)
	: Connector(pluggingController_, std::move(name_),
	            std::make_unique<DummyRS232Device>())
{
}

std::string_view RS232Connector::getDescription() const
{
	return "Serial RS232 connector";
}

std::string_view RS232Connector::getClass() const
{
	return "RS232";
}

RS232Device& RS232Connector::getPluggedRS232Dev() const
{
	return *checked_cast<RS232Device*>(&getPlugged());
}

template<typename Archive>
void RS232Connector::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<Connector>(*this);
}
INSTANTIATE_SERIALIZE_METHODS(RS232Connector);

} // namespace openmsx
