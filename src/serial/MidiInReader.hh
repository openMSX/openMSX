// $Id$

#ifndef __MIDIINREADER_HH__
#define __MIDIINREADER_HH__

#include <stdio.h>
#include <list>
#include "openmsx.hh"
#include "MidiInDevice.hh"
#include "Thread.hh"
#include "Schedulable.hh"
#include "Semaphore.hh"

using std::list;

class MidiInConnector;


class MidiInReader : public MidiInDevice, private Runnable, private Schedulable
{
	public:
		MidiInReader();
		virtual ~MidiInReader();
		
		// Pluggable
		virtual void plug(Connector* connector, const EmuTime& time);
		virtual void unplug(const EmuTime& time);
		virtual const string &getName() const;
		
		// MidiInDevice
		virtual void signal(const EmuTime& time);
	
	private:
		// Runnable
		virtual void run();

		// Schedulable
		virtual void executeUntilEmuTime(const EmuTime& time, int userData);
		virtual const string& schedName() const;
		
		Thread thread;
		FILE* file;
		MidiInConnector* connector;
		list<byte> queue;
		Semaphore lock; // to protect queue
		
};

#endif

