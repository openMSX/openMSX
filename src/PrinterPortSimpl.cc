#include "PrinterPortSimpl.hh"
#include "DeviceConfig.hh"
#include "XMLElement.hh"
#include "serialize.hh"
#include <memory>

namespace openmsx {

static constexpr static_string_view DESCRIPTION =
	"Play samples via your printer port.";

PrinterPortSimpl::PrinterPortSimpl(const HardwareConfig& hwConf_)
	: hwConf(hwConf_)
{
}

bool PrinterPortSimpl::getStatus(EmuTime::param /*time*/)
{
	return true; // TODO check
}

void PrinterPortSimpl::setStrobe(bool /*strobe*/, EmuTime::param /*time*/)
{
	// ignore strobe // TODO check
}

void PrinterPortSimpl::writeData(uint8_t data, EmuTime::param time)
{
	assert(dac);
	dac->writeDAC(data, time);
}

void PrinterPortSimpl::createDAC()
{
	static const XMLElement* xml = [] {
		auto& doc = XMLDocument::getStaticDocument();
		auto* result = doc.allocateElement("simpl");
		result->setFirstChild(doc.allocateElement("sound"))
		      ->setFirstChild(doc.allocateElement("volume", "12000"));
		return result;
	}();
	dac.emplace("simpl", DESCRIPTION, DeviceConfig(hwConf, *xml));
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
		if constexpr (Archive::IS_LOADER) {
			createDAC();
		}
		ar.serialize("dac", *dac);
	}
}
INSTANTIATE_SERIALIZE_METHODS(PrinterPortSimpl);
REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, PrinterPortSimpl, "PrinterPortSimpl");

} // namespace openmsx
