// $Id$

#include "MSXAudio.hh"
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
	
	MSXConfig *config = MSXConfig::instance();
	config->loadStream(s);
}
const std::string& MSXAudioCLI::optionHelp() const
{
	static const std::string text("Inserts a Philips Music Module (rom disabled)");
	return text;
}


MSXAudio::MSXAudio(Device *config, const EmuTime &time)
	: MSXDevice(config, time), MSXIODevice(config, time)
{
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

void MSXAudio::powerOff(const EmuTime &time)
{
	y8950->powerOff(time);
}

void MSXAudio::reset(const EmuTime &time)
{
	y8950->reset(time);
	registerLatch = 0;	// TODO check
}

byte MSXAudio::readIO(byte port, const EmuTime &time)
{
	byte result;
	switch (port & 0x01) {
	case 0:
		result = y8950->readStatus();
		break;
	case 1:
		result = y8950->readReg(registerLatch, time);
		break;
	default: // unreachable, avoid warning
		assert(false);
		result = 0;
	}
	//PRT_DEBUG("Audio: read "<<std::hex<<(int)port<<" "<<(int)result<<std::dec);
	return result;
}

void MSXAudio::writeIO(byte port, byte value, const EmuTime &time)
{
	//PRT_DEBUG("Audio: write "<<std::hex<<(int)port<<" "<<(int)value<<std::dec);
	switch (port & 0x01) {
	case 0:
		registerLatch = value;
		break;
	case 1:
		y8950->writeReg(registerLatch, value, time);
		break;
	default:
		assert(false);
	}
}
