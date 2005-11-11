// $Id$

#include "PrinterPortSimpl.hh"
#include "DACSound8U.hh"
#include "XMLElement.hh"

namespace openmsx {

PrinterPortSimpl::PrinterPortSimpl(Mixer& mixer_)
	: mixer(mixer_)
{
}

bool PrinterPortSimpl::getStatus(const EmuTime& /*time*/)
{
	return true;	// TODO check
}

void PrinterPortSimpl::setStrobe(bool /*strobe*/, const EmuTime& /*time*/)
{
	// ignore strobe	TODO check
}

void PrinterPortSimpl::writeData(byte data, const EmuTime& time)
{
	dac->writeDAC(data, time);
}

void PrinterPortSimpl::plugHelper(Connector& /*connector*/, const EmuTime& time)
{
	// TODO get from config file
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
	dac.reset(new DACSound8U(mixer, "simpl", getDescription(),
	          simplConfig, time));
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

} // namespace openmsx
