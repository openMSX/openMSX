// $Id$

#ifndef __MIDIINNATIVE_HH__
#define __MIDIINNATIVE_HH__

#if defined(__WIN32__)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <list>
#include "openmsx.hh"
#include "MidiInDevice.hh"
#include "Thread.hh"
#include "Schedulable.hh"
#include "Semaphore.hh"
#include <SDL_thread.h>
#include <windows.h>
#include <mmsystem.h>

using std::list;

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
	virtual const string& getName() const;
	virtual const string& getDescription() const;

	// MidiInDevice
	virtual void signal(const EmuTime& time);

private:
	// Runnable
	virtual void run();

	// Schedulable
	virtual void executeUntil(const EmuTime& time, int userData) throw();
	virtual const string& schedName() const;

	void procShortMsg(long unsigned int param);
	void procLongMsg(LPMIDIHDR p);
	
	Thread thread;
	unsigned int devidx;
	unsigned int thrdid;
	list<byte> queue;
	Semaphore lock; // to protect queue
	string name;
	string desc;
};

} // namespace openmsx

#endif // defined(__WIN32__)
#endif // __MIDIINNATIVE_HH__

