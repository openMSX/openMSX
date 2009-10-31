// $Id$

#ifndef REVERSEMANGER_HH
#define REVERSEMANGER_HH

#include "Schedulable.hh"
#include "MSXEventListener.hh"
#include "EmuTime.hh"
#include "Event.hh"
#include "shared_ptr.hh"
#include <vector>
#include <map>
#include <memory>

namespace openmsx {

class MSXMotherBoard;
class ReverseCmd;
class MemBuffer;

class ReverseManager : private Schedulable, private MSXEventListener
{
public:
	ReverseManager(MSXMotherBoard& motherBoard);
	~ReverseManager();

private:
	struct ReverseChunk {
		ReverseChunk() : time(EmuTime::zero) {}

		EmuTime time;
		// TODO use unique_ptr in the future (c++0x), or hold
		//      MemBuffer by value and make it moveable
		shared_ptr<MemBuffer> savestate;

		// Number of recorded events (or replay index) when this
		// snapshot was created. So when going back replay should
		// start at this index.
		unsigned eventCount;
	};
	typedef std::map<unsigned, ReverseChunk> Chunks;

	struct EventChunk {
		EventChunk(EmuTime time_, shared_ptr<const Event> event_)
			: time(time_)
			, event(event_) {}
		EmuTime time;
		shared_ptr<const Event> event;
	};
	typedef std::vector<EventChunk> Events;

	struct ReverseHistory {
		void swap(ReverseHistory& other);
		void clear();

		Chunks chunks;
		Events events;
	};

	bool collecting() const;
	bool replaying() const;

	void start();
	void stop();
	std::string status();
	void go(const std::vector<std::string>& tokens);
	void goBack(const std::vector<std::string>& tokens);
	
	void goToSnapshot(Chunks::iterator chunk_it);

	void transferHistory(ReverseHistory& oldHistory,
                             unsigned oldCollectCount,
                             unsigned oldEventCount);
	void schedule(EmuTime::param time);
	void replayNextEvent();
	template<unsigned N> void dropOldSnapshots(unsigned count);

	// Schedulable
	virtual void executeUntil(EmuTime::param time, int userData);
	virtual const std::string& schedName() const;

	// EventListener
	virtual void signalEvent(shared_ptr<const Event> event,
                                 EmuTime::param time);

	MSXMotherBoard& motherBoard;
	const std::auto_ptr<ReverseCmd> reverseCmd;
	ReverseHistory history;
	unsigned collectCount; // 0     = not collecting
	                       // other = number of snapshot that's about to be taken
	unsigned replayIndex;

	friend class ReverseCmd;
};

} // namespace openmsx

#endif
