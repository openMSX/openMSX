// $Id$

#ifndef __SCHEDULER_HH__
#define __SCHEDULER_HH__

#include "EmuTime.hh"
#include <vector>

class MSXCPU;
class Schedulable;


class Scheduler
{
	class SynchronizationPoint
	{
		public:
			SynchronizationPoint (const EmuTime &time, Schedulable *dev, int usrdat) :
				timeStamp(time), device(dev) , userData(usrdat) {}
			const EmuTime &getTime() const { return timeStamp; }
			Schedulable *getDevice() const { return device; }
			int getUserData() const { return userData; }
			bool operator< (const SynchronizationPoint &n) const
				{ return getTime() > n.getTime(); } // smaller time is higher priority
		private:
			EmuTime timeStamp;
			Schedulable* device;
			int userData;
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
		 * Register a syncPoint. When the emulation reaches "timestamp",
		 * the executeUntilEmuTime() method of "device" gets called.
		 * SyncPoints are ordered: smaller EmuTime -> scheduled
		 * earlier.
		 * The supplied EmuTime may not be smaller than the current CPU
		 * time.
		 * If you want to schedule something as soon as possible, you
		 * can pass Scheduler::ASAP as time argument.
		 * A device may register several syncPoints.
		 * Optionally a "userData" parameter can be passed, this
		 * parameter is not used by the Scheduler but it is passed to
		 * the executeUntilEmuTime() method of "device". This is useful
		 * if you want to distinguish between several syncPoint types.
		 * If you do not supply "userData" it is assumed to be zero.
		 */
		void setSyncPoint(const EmuTime &timestamp, Schedulable* device, int userData = 0);

		/**
		 * Removes a syncPoint of a given device that matches the given
		 * userData.
		 * If there is more than one match only one will be removed,
		 * there is no guarantee that the earliest syncPoint is
		 * removed.
		 * Removing a syncPoint is a relatively expensive operation,
		 * if possible don't remove the syncPoint but ignore it in
		 * your executeUntilEmuTime() method.
		 */
		void removeSyncPoint(Schedulable* device, int userdata = 0);

		/**
		 * This starts the schedule loop, should only be used by main
		 * the program.
		 * @result The moment in time the scheduler stopped.
		 */
		const EmuTime scheduleEmulation();

		/**
		 * This stops the schedule loop, should only be used by the
		 * quit program routine.
		 */
		void stopScheduling();

		/**
		 * This (un)pauses the emulation.
		 */
		void pause();
		void unpause();

		/** Should the emulation continue running?
		  * @return True iff emulation should continue.
		  */
		bool isEmulationRunning() {
			return emulationRunning;
		}

		static const EmuTime ASAP;

	private:
		Scheduler();

		/** Runs a single emulation step.
		  */
		inline void emulateStep();

		/** Vector used as heap, not a priority queue because that
		  * doesn't allow removal of non-top element.
		  */
		std::vector<SynchronizationPoint> syncPoints;

		/** Should the emulation continue running?
		  */
		volatile bool emulationRunning;

		bool paused;
		MSXCPU *cpu;

};

#endif
