// $Id$

#ifndef __MIDIOUTLOGGER_HH__
#define __MIDIOUTLOGGER_HH__

#include "MidiOutDevice.hh"
#include "StringSetting.hh"
#include <fstream>

namespace openmsx {

class MidiOutLogger : public MidiOutDevice
{
public:
	MidiOutLogger();
	virtual ~MidiOutLogger();

	// Pluggable
	virtual void plugHelper(Connector* connector, const EmuTime& time);
	virtual void unplugHelper(const EmuTime& time);
	virtual const std::string& getName() const;
	virtual const std::string& getDescription() const;

	// SerialDataInterface (part)
	virtual void recvByte(byte value, const EmuTime& time);

private:
	StringSetting logFilenameSetting;
	std::ofstream file;
};

} // namespace openmsx

#endif

