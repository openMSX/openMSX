// $Id$

#include "MidiOutLogger.hh"
#include "PluggingController.hh"

namespace openmsx {

MidiOutLogger::MidiOutLogger()
	: logFilenameSetting("midi-out-logfilename",
                "filename of the file where the MIDI output is logged to",
                "/dev/midi")

{
	PluggingController::instance()->registerPluggable(this);
}

MidiOutLogger::~MidiOutLogger()
{
	PluggingController::instance()->unregisterPluggable(this);
}

void MidiOutLogger::plug(Connector* connector, const EmuTime& time)
{
	file.open(logFilenameSetting.getValue().c_str());
}

void MidiOutLogger::unplug(const EmuTime &time)
{
	file.close();
}

const string& MidiOutLogger::getName() const
{
	static const string name("midi-out-logger");
	return name;
}

void MidiOutLogger::recvByte(byte value, const EmuTime& time)
{
	file.put(value);
	file.flush();
}

} // namespace openmsx
