// $Id$

#ifndef __MIDIOUTLOGGER_HH__
#define __MIDIOUTLOGGER_HH__

#include <fstream>
#include "MidiOutDevice.hh"
#include "StringSetting.hh"

using std::ofstream;

namespace openmsx {

class MidiOutLogger : public MidiOutDevice
{
public:
	MidiOutLogger();
	virtual ~MidiOutLogger();

	// Pluggable
	virtual void plugHelper(Connector* connector, const EmuTime& time)
		throw(PlugException);
	virtual void unplugHelper(const EmuTime& time);
	virtual const string& getName() const;
	virtual const string& getDescription() const;

	// SerialDataInterface (part)
	virtual void recvByte(byte value, const EmuTime& time);

private:
	StringSetting logFilenameSetting;
	ofstream file;
};

} // namespace openmsx

#endif

