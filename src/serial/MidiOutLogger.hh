// $Id$

#ifndef MIDIOUTLOGGER_HH
#define MIDIOUTLOGGER_HH

#include "MidiOutDevice.hh"
#include "serialize_meta.hh"
#include <fstream>
#include <memory>

namespace openmsx {

class CommandController;
class FilenameSetting;

class MidiOutLogger : public MidiOutDevice
{
public:
	explicit MidiOutLogger(CommandController& commandController);

	// Pluggable
	virtual void plugHelper(Connector& connector, const EmuTime& time);
	virtual void unplugHelper(const EmuTime& time);
	virtual const std::string& getName() const;
	virtual const std::string& getDescription() const;

	// SerialDataInterface (part)
	virtual void recvByte(byte value, const EmuTime& time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	const std::auto_ptr<FilenameSetting> logFilenameSetting;
	std::ofstream file;
};

REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, MidiOutLogger, "MidiOutLogger");

} // namespace openmsx

#endif
