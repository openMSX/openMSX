// $Id$

#ifndef __SCHEDULER_HH__
#define __SCHEDULER_HH__

#include "EmuTime.hh"
#include "HotKey.hh"
#include <SDL/SDL.h>
#include <set>
#include <string>


class Schedulable
{
	public:
		virtual void executeUntilEmuTime(const EmuTime &time) = 0;
		virtual const std::string &getName();
	protected:
		static const std::string defaultName;
};


class SynchronizationPoint
{
	public:
		SynchronizationPoint (const EmuTime &time, Schedulable &dev) : timeStamp(time), device(dev) {}
		const EmuTime &getTime() const { return timeStamp; }
		Schedulable &getDevice() const { return device; }
		bool operator< (const SynchronizationPoint &n) const { return getTime() < n.getTime(); }
	private: 
		const EmuTime timeStamp;	// copy of original timestamp
		Schedulable &device;		// alias
};

class Scheduler : public EventListener, public HotKeyListener
{
	public:
		virtual ~Scheduler();
		static Scheduler *instance();
		void setSyncPoint(const EmuTime &timestamp, Schedulable &activedevice);
		void scheduleEmulation();
		void stopScheduling();
		// EventListener
		void signalEvent(SDL_Event &event);
		// HotKeyListener
		void signalHotKey(SDLKey key);

		void pause();
		void unpause();
		
	private:
		Scheduler();
		const SynchronizationPoint &getFirstSP();
		void removeFirstSP();
		
		static Scheduler *oneInstance;
		std::set<SynchronizationPoint> scheduleList;

		static const EmuTime infinity;

		bool paused;
		bool runningScheduler;
		SDL_mutex *pauseMutex;
};

#endif
