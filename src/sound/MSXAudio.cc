// $Id$

#include "MSXAudio.hh"
#include "MSXCPUInterface.hh"
#include "Mixer.hh"
#include "Y8950.hh"
#include "MSXConfig.hh"


MSXAudioCLI msxAudioCLI;

MSXAudioCLI::MSXAudioCLI()
{
	CommandLineParser::instance()->registerOption("-musmod", this);
}

void MSXAudioCLI::parseOption(const std::string &option,
                              std::list<std::string> &cmdLine)
{
	std::ostringstream s;
	s << "<?xml version=\"1.0\"?>";
	s << "<msxconfig>";
	s << "<device id=\"MSX-Audio\">";
	s << "<type>Audio</type>";
	s << "<parameter name=\"volume\">9000</parameter>";
	s << "<parameter name=\"sampleram\">256</parameter>";
	s << "<parameter name=\"mode\">mono</parameter>";
	s << "</device>";
	s << "<device id=\"MSX-Audio-Midi\">";
	s << "<type>Audio-Midi</type>";
	s << "</device>";
	s << "</msxconfig>";
	
	MSXConfig::Backend *config = MSXConfig::Backend::instance();
	config->loadStream(s);
}
const std::string& MSXAudioCLI::optionHelp()
{
	static const std::string text("Inserts a Philips Music Module (rom disabled)");
	return text;
}


MSXAudio::MSXAudio(MSXConfig::Device *config, const EmuTime &time)
	: MSXDevice(config, time), MSXIODevice(config, time)
{
	if (config->hasParameter("number") &&
	    config->getParameter("number") == "2") {
		base = 0xC2;
	} else {
		base = 0xC0;
	}
	MSXCPUInterface::instance()->register_IO_Out(base + 0, this);
	MSXCPUInterface::instance()->register_IO_Out(base + 1, this);
	MSXCPUInterface::instance()->register_IO_In (base + 0, this);
	MSXCPUInterface::instance()->register_IO_In (base + 1, this);
	short volume = (short)deviceConfig->getParameterAsInt("volume");
	
	// left / right / mono
	Mixer::ChannelMode mode = Mixer::MONO;
	try {
		std::string stereomode = config->getParameter("mode");
		if      (stereomode == "left")
			mode=Mixer::MONO_LEFT;
		else if (stereomode == "right")
			mode=Mixer::MONO_RIGHT;
	} catch (MSXException &e) {
		PRT_ERROR("Exception: " << e.desc);
	}
	
	// SampleRAM size
	int ramSize = 256;	// in kb
	try {
		ramSize = config->getParameterAsInt("sampleram");
	} catch (MSXException &e) {
		//  no size specified
	}
	
	y8950 = new Y8950(volume, ramSize * 1024, time, mode);
	reset(time);
}

MSXAudio::~MSXAudio()
{
	delete y8950;
}

void MSXAudio::reset(const EmuTime &time)
{
	y8950->reset(time);
	registerLatch = 0;	// TODO check
}

byte MSXAudio::readIO(byte port, const EmuTime &time)
{
	byte result;
	if (port == base + 0) {
		result = y8950->readStatus();
	} else if (port == base + 1) {
		result = y8950->readReg(registerLatch, time);
	} else {
		assert(false);
		result = 0;	// avoid warning
	}
	//PRT_DEBUG("Audio: read "<<std::hex<<(int)port<<" "<<(int)result<<std::dec);
	return result;
}

void MSXAudio::writeIO(byte port, byte value, const EmuTime &time)
{
	//PRT_DEBUG("Audio: write "<<std::hex<<(int)port<<" "<<(int)value<<std::dec);
	if (port == base + 0) {
		registerLatch = value;
	} else if (port == base + 1) {
		y8950->writeReg(registerLatch, value, time);
	}
}
