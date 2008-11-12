// $Id$

#include "PrinterPortSimpl.hh"
#include "DACSound8U.hh"
#include "XMLElement.hh"
#include "serialize.hh"

namespace openmsx {

PrinterPortSimpl::PrinterPortSimpl(MSXMixer& mixer_)
	: mixer(mixer_)
{
}

bool PrinterPortSimpl::getStatus(const EmuTime& /*time*/)
{
	return true; // TODO check
}

void PrinterPortSimpl::setStrobe(bool /*strobe*/, const EmuTime& /*time*/)
{
	// ignore strobe // TODO check
}

void PrinterPortSimpl::writeData(byte data, const EmuTime& time)
{
	dac->writeDAC(data, time);
}

void PrinterPortSimpl::createDAC()
{
	static XMLElement simplConfig("simpl");
	static bool init = false;
	if (!init) {
		init = true;
		std::auto_ptr<XMLElement> soundElem(new XMLElement("sound"));
		soundElem->addChild(std::auto_ptr<XMLElement>(
			new XMLElement("volume", "12000")));
		soundElem->addChild(std::auto_ptr<XMLElement>(
			new XMLElement("mode", "mono")));
		simplConfig.addChild(soundElem);
	}
	dac.reset(new DACSound8U(mixer, "simpl", getDescription(), simplConfig));
}

void PrinterPortSimpl::plugHelper(Connector& /*connector*/, const EmuTime& /*time*/)
{
	createDAC();
}

void PrinterPortSimpl::unplugHelper(const EmuTime& /*time*/)
{
	dac.reset();
}

const std::string& PrinterPortSimpl::getName() const
{
	static const std::string name("simpl");
	return name;
}

const std::string& PrinterPortSimpl::getDescription() const
{
	static const std::string desc("Play samples via your printer port.");
	return desc;
}

template<typename Archive>
void PrinterPortSimpl::serialize(Archive& ar, unsigned /*version*/)
{
	if (getConnector()) {
		// plugged in
		if (ar.isLoader()) {
			createDAC();
		}
		ar.serialize("dac", *dac);
	}
}
INSTANTIATE_SERIALIZE_METHODS(PrinterPortSimpl);
REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, PrinterPortSimpl, "PrinterPortSimpl");

} // namespace openmsx
