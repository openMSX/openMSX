// $Id$

#ifndef __MIDIINREADER_HH__
#define __MIDIINREADER_HH__

#include <cstdio>
#include <list>
#include "openmsx.hh"
#include "MidiInDevice.hh"
#include "Thread.hh"
#include "Schedulable.hh"
#include "Semaphore.hh"
#include "StringSetting.hh"

using std::list;


namespace openmsx {

class MidiInConnector;


class MidiInReader : public MidiInDevice, private Runnable, private Schedulable
{
public:
	MidiInReader();
	virtual ~MidiInReader();

	// Pluggable
	virtual void plug(Connector* connector, const EmuTime& time)
		throw(PlugException);
	virtual void unplug(const EmuTime& time);
	virtual const string& getName() const;
	virtual const string& getDescription() const;

	// MidiInDevice
	virtual void signal(const EmuTime& time);

private:
	// Runnable
	virtual void run() throw();

	// Schedulable
	virtual void executeUntil(const EmuTime& time, int userData) throw();
	virtual const string& schedName() const;

	Thread thread;
	FILE* file;
	MidiInConnector* connector;
	list<byte> queue;
	Semaphore lock; // to protect queue

	StringSetting readFilenameSetting;
};

} // namespace openmsx

#endif

