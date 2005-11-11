// $Id$

#include "MidiOutLogger.hh"
#include "FilenameSetting.hh"

namespace openmsx {

MidiOutLogger::MidiOutLogger(CommandController& commandController)
	: logFilenameSetting(new FilenameSetting(
		commandController, "midi-out-logfilename",
		"filename of the file where the MIDI output is logged to",
		"/dev/midi"))
{
}

void MidiOutLogger::plugHelper(Connector& /*connector*/,
                               const EmuTime& /*time*/)
{
	file.open(logFilenameSetting->getValue().c_str());
	if (file.fail()) {
		file.clear();
		throw PlugException("Error opening log file");
	}
}

void MidiOutLogger::unplugHelper(const EmuTime& /*time*/)
{
	file.close();
}

const std::string& MidiOutLogger::getName() const
{
	static const std::string name("midi-out-logger");
	return name;
}

const std::string& MidiOutLogger::getDescription() const
{
	static const std::string desc(
		"Midi output logger. Log all data that is sent to this "
		"pluggable to a file. The filename is set with the "
		"'midi-out-logfilename' setting.");
	return desc;
}

void MidiOutLogger::recvByte(byte value, const EmuTime& /*time*/)
{
	file.put(value);
	file.flush();
}

} // namespace openmsx
