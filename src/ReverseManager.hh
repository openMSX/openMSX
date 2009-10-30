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

		// index of last event that was processed before making snapshot
		unsigned nextEventIndex;
	};
	typedef std::map<unsigned, ReverseChunk> Chunks;

	struct EventChunk {
		EventChunk(EmuTime time_, shared_ptr<const Event> event_)
                        : time(time_)
                        , event(event_) {}
		EmuTime getTime() const { return time; }
		shared_ptr<const Event> getEvent() const { return event; }
		private:
		EmuTime time;
		shared_ptr<const Event> event;
	};
	typedef std::vector<EventChunk> Events;

	struct ReverseHistory {
		ReverseHistory();
		void swap(ReverseHistory& other);
		void clear();

		Chunks chunks;
		Events events;
	};

	std::string start();
	std::string stop();
	std::string status();
	std::string go(const std::vector<std::string>& tokens);
	std::string goBack(const std::vector<std::string>& tokens);
	
	void goToSnapshot(Chunks::iterator chunk_it);

	void transferHistory(ReverseHistory& oldHistory,
                             unsigned oldCollectCount,
                             unsigned eventHistoryIndex);
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
	unsigned currentEventReplayIndex;

	friend class ReverseCmd;
};

} // namespace openmsx

#endif
