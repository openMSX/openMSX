// $Id$

#ifndef MIDIOUTLOGGER_HH
#define MIDIOUTLOGGER_HH

#include "MidiOutDevice.hh"
#include <fstream>
#include <memory>

namespace openmsx {

class FilenameSetting;

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
	const std::auto_ptr<FilenameSetting> logFilenameSetting;
	std::ofstream file;
};

} // namespace openmsx

#endif
