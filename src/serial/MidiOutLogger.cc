// $Id$

#include "MidiOutLogger.hh"
#include "PluggingController.hh"

MidiOutLogger::MidiOutLogger()
{
	PluggingController::instance()->registerPluggable(this);
}

MidiOutLogger::~MidiOutLogger()
{
	PluggingController::instance()->unregisterPluggable(this);
}

void MidiOutLogger::plug(Connector* connector, const EmuTime& time)
{
	file.open("/dev/midi");
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
}
