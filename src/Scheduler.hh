// $Id$

#ifndef __SCHEDULER_HH__
#define __SCHEDULER_HH__

#include "EmuTime.hh"
#include "HotKey.hh"
#include <SDL/SDL.h>
#include <set>
#include <string>

/**
 * Every class that wants to get scheduled at some point must inherit from
 * this class
 */
class Schedulable
{
	public:
		/**
		 * When the previously registered syncPoint is reached, this
		 * method gets called.
		 */
		virtual void executeUntilEmuTime(const EmuTime &time) = 0;
		
		/**
		 * This method is only used to print meaningfull debug messages
		 */
		virtual const std::string &getName();
		
	protected:
		static const std::string defaultName;
};

class Scheduler : public EventListener, public HotKeyListener, public Schedulable
{
	class SynchronizationPoint
	{
		public:
			SynchronizationPoint (const EmuTime &time, Schedulable &dev) : 
				timeStamp(time), device(dev) {}
			const EmuTime &getTime() const { return timeStamp; }
			Schedulable &getDevice() const { return device; }
			bool operator< (const SynchronizationPoint &n) const 
				{ return getTime() < n.getTime(); }
		private: 
			const EmuTime timeStamp;	// copy of original timestamp
			Schedulable &device;		// alias
	};

	public:
		/**
		 * Destructor
		 */
		virtual ~Scheduler();

		/**
		 * This is a singleton class,
		 *  usage: Scheduler::instance()->method();
		 */
		static Scheduler *instance();

		/**
		 * Register a given syncPoint. When the emulation reaches
		 * "timestamp", the executeUntillEmuTime() method of
		 * "activeDevice" gets called.
		 * All syncPoints are ordered: smaller EmuTime -> scheduled
		 * earlier.
		 * One device may register several syncPoints, all syncPoints
		 * get called.
		 */
		void setSyncPoint(const EmuTime &timestamp, Schedulable &activeDevice);
		
		/**
		 * This starts the schedule loop, should only be used by main
		 * the program.
		 */
		void scheduleEmulation();

		/**
		 * This stops the schedule loop, should only be used by the 
		 * quit program routine.
		 */
		void stopScheduling();

		/**
		 * This pauses the emulation.
		 */
		void pause();

		/**
		 * This unpauses the emulation
		 */
		void unpause();
		
		
		// Schedulable
		void executeUntilEmuTime(const EmuTime &time);
		// EventListener
		void signalEvent(SDL_Event &event);
		// HotKeyListener
		void signalHotKey(SDLKey key);
		
	private:
		Scheduler();
		const SynchronizationPoint &getFirstSP();
		void removeFirstSP();
		
		static Scheduler *oneInstance;
		std::set<SynchronizationPoint> syncPoints;	// a std::set is sorted

		bool paused;
		bool noSound;
		bool runningScheduler;
		SDL_mutex *pauseMutex;
};

#endif
