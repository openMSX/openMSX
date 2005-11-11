// $Id$

#ifndef MIDIINREADER_HH
#define MIDIINREADER_HH

#include "MidiInDevice.hh"
#include "Thread.hh"
#include "Schedulable.hh"
#include "Semaphore.hh"
#include "openmsx.hh"
#include <cstdio>
#include <deque>
#include <memory>

namespace openmsx {

class CommandController;
class FilenameSetting;

class MidiInReader : public MidiInDevice, private Runnable, private Schedulable
{
public:
	MidiInReader(Scheduler& scheduler, CommandController& commandController);
	virtual ~MidiInReader();

	// Pluggable
	virtual void plugHelper(Connector& connector, const EmuTime& time);
	virtual void unplugHelper(const EmuTime& time);
	virtual const std::string& getName() const;
	virtual const std::string& getDescription() const;

	// MidiInDevice
	virtual void signal(const EmuTime& time);

private:
	// Runnable
	virtual void run();

	// Schedulable
	virtual void executeUntil(const EmuTime& time, int userData);
	virtual const std::string& schedName() const;

	Thread thread;
	FILE* file;
	std::deque<byte> queue;
	Semaphore lock; // to protect queue

	const std::auto_ptr<FilenameSetting> readFilenameSetting;
};

} // namespace openmsx

#endif
