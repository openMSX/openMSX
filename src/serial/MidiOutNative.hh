// $Id$

#ifndef __MIDIOUTNATIVE_HH__
#define __MIDIOUTNATIVE_HH__

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
	virtual const string& getName() const;
	virtual const string& getDescription() const;

	// SerialDataInterface (part)
	virtual void recvByte(byte value, const EmuTime& time);

private:
	unsigned devidx;
	string name;
	string desc;
};

} // namespace openmsx

#endif // defined(_WIN32)
#endif // __MIDIOUTNATIVE_HH__

