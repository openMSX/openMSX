// $Id$

#include "MSXRTC.hh"
#include "RP5C01.hh"
#include "File.hh"
#include "MSXConfig.hh"


MSXRTC::MSXRTC(Device *config, const EmuTime &time)
	: MSXDevice(config, time), MSXIODevice(config, time)
{
	bool emuTimeBased = deviceConfig->getParameter("mode") != "RealTime";
	try {
		if (deviceConfig->getParameterAsBool("load")) {
			const std::string &filename = deviceConfig->getParameter("filename");
			File file(deviceConfig->getContext(), filename);
			byte buffer[4 * 13];
			file.read(buffer, 4 * 13);
			rp5c01 = new RP5C01(emuTimeBased, buffer, time);	// use data from buffer
		} else {
			throw FileException("dummy");	// use default values
		}
	} catch (FileException &e) {
		rp5c01 = new RP5C01(emuTimeBased, time);	// use default values
	}
	reset(time);
}

MSXRTC::~MSXRTC()
{
	if (deviceConfig->getParameterAsBool("save")) {
		const std::string &filename = deviceConfig->getParameter("filename");
		File file(deviceConfig->getContext(), filename, TRUNCATE);
		file.write(rp5c01->getRegs(), 4 * 13);
	}
	delete rp5c01;
}

void MSXRTC::reset(const EmuTime &time)
{
	rp5c01->reset(time);
}

byte MSXRTC::readIO(byte port, const EmuTime &time)
{
	return rp5c01->readPort(registerLatch, time) | 0xf0;
}

void MSXRTC::writeIO(byte port, byte value, const EmuTime &time)
{
	switch (port & 0x01) {
	case 0:
		registerLatch = value & 0x0f;
		break;
	case 1:
		rp5c01->writePort(registerLatch, value & 0x0f, time);
		break;
	default:
		assert(false);
	}
}

