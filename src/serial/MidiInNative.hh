// $Id$

#ifndef __MIDIINNATIVE_HH__
#define __MIDIINNATIVE_HH__

#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "openmsx.hh"
#include "MidiInDevice.hh"
#include "Thread.hh"
#include "Schedulable.hh"
#include "Semaphore.hh"
#include <SDL_thread.h>
#include <windows.h>
#include <mmsystem.h>
#include <deque>

namespace openmsx {

class PluggingController;

class MidiInNative : public MidiInDevice, private Runnable, private Schedulable
{
public:
	/** Register all available native midi in devcies
	  */
	static void registerAll(PluggingController* controller);
	
	MidiInNative(unsigned);
	virtual ~MidiInNative();

	// Pluggable
	virtual void plugHelper(Connector* connector, const EmuTime& time);
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

	void procShortMsg(long unsigned int param);
	void procLongMsg(LPMIDIHDR p);
	
	Thread thread;
	unsigned int devidx;
	unsigned int thrdid;
	std::deque<byte> queue;
	Semaphore lock; // to protect queue
	std::string name;
	std::string desc;
};

} // namespace openmsx

#endif // defined(_WIN32)
#endif // __MIDIINNATIVE_HH__

