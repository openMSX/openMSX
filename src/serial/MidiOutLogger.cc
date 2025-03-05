#include "MidiOutLogger.hh"
#include "PlugException.hh"
#include "FileOperations.hh"
#include "narrow.hh"
#include "serialize.hh"

namespace openmsx {

MidiOutLogger::MidiOutLogger(CommandController& commandController)
	: logFilenameSetting(
		commandController, "midi-out-logfilename",
		"filename of the file where the MIDI output is logged to",
		"/dev/midi")
{
}

void MidiOutLogger::plugHelper(Connector& /*connector*/,
                               EmuTime::param /*time*/)
{
	FileOperations::openOfStream(file, logFilenameSetting.getString());
	if (file.fail()) {
		file.clear();
		throw PlugException("Error opening log file");
	}
}

void MidiOutLogger::unplugHelper(EmuTime::param /*time*/)
{
	file.close();
}

std::string_view MidiOutLogger::getName() const
{
	return "midi-out-logger";
}

std::string_view MidiOutLogger::getDescription() const
{
	return "Midi output logger. Log all data that is sent to this "
	       "pluggable to a file. The filename is set with the "
	       "'midi-out-logfilename' setting.";
}

void MidiOutLogger::recvByte(uint8_t value, EmuTime::param /*time*/)
{
	if (file.is_open()) {
		file.put(narrow_cast<char>(value));
		file.flush();
	}
}

template<typename Archive>
void MidiOutLogger::serialize(Archive& /*ar*/, unsigned /*version*/)
{
	// don't try to resume a previous logfile (see PrinterPortLogger)
}
INSTANTIATE_SERIALIZE_METHODS(MidiOutLogger);
REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, MidiOutLogger, "MidiOutLogger");

} // namespace openmsx
