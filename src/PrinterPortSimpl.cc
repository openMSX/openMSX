#include "PrinterPortSimpl.hh"
#include "DACSound8U.hh"
#include "DeviceConfig.hh"
#include "XMLElement.hh"
#include "serialize.hh"
#include <memory>

namespace openmsx {

constexpr static_string_view DESCRIPTION =
	"Play samples via your printer port.";

PrinterPortSimpl::PrinterPortSimpl(const HardwareConfig& hwConf_)
	: hwConf(hwConf_)
{
}

PrinterPortSimpl::~PrinterPortSimpl() = default;

bool PrinterPortSimpl::getStatus(EmuTime::param /*time*/)
{
	return true; // TODO check
}

void PrinterPortSimpl::setStrobe(bool /*strobe*/, EmuTime::param /*time*/)
{
	// ignore strobe // TODO check
}

void PrinterPortSimpl::writeData(byte data, EmuTime::param time)
{
	dac->writeDAC(data, time);
}

static XMLElement createXML()
{
	XMLElement xml("simpl");
	xml.addChild("sound").addChild("volume", "12000");
	return xml;
}

void PrinterPortSimpl::createDAC()
{
	static XMLElement xml = createXML();
	dac = std::make_unique<DACSound8U>("simpl", DESCRIPTION,
	                                   DeviceConfig(hwConf, xml));
}

void PrinterPortSimpl::plugHelper(Connector& /*connector*/, EmuTime::param /*time*/)
{
	createDAC();
}

void PrinterPortSimpl::unplugHelper(EmuTime::param /*time*/)
{
	dac.reset();
}

std::string_view PrinterPortSimpl::getName() const
{
	return "simpl";
}

std::string_view PrinterPortSimpl::getDescription() const
{
	return DESCRIPTION;
}

template<typename Archive>
void PrinterPortSimpl::serialize(Archive& ar, unsigned /*version*/)
{
	if (isPluggedIn()) {
		if (ar.isLoader()) {
			createDAC();
		}
		ar.serialize("dac", *dac);
	}
}
INSTANTIATE_SERIALIZE_METHODS(PrinterPortSimpl);
REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, PrinterPortSimpl, "PrinterPortSimpl");

} // namespace openmsx
