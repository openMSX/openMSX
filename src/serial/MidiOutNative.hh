// $Id$

#ifndef MIDIOUTNATIVE_HH
#define MIDIOUTNATIVE_HH

#if defined(_WIN32)
#include "MidiOutDevice.hh"

namespace openmsx {

class PluggingController;

class MidiOutNative : public MidiOutDevice
{
public:
	static void registerAll(PluggingController* controller);

	MidiOutNative(unsigned num);
	virtual ~MidiOutNative();

	// Pluggable
	virtual void plugHelper(Connector* connector, const EmuTime& time);
	virtual void unplugHelper(const EmuTime& time);
	virtual const std::string& getName() const;
	virtual const std::string& getDescription() const;

	// SerialDataInterface (part)
	virtual void recvByte(byte value, const EmuTime& time);

private:
	unsigned devidx;
	std::string name;
	std::string desc;
};

} // namespace openmsx

#endif // defined(_WIN32)
#endif // MIDIOUTNATIVE_HH
