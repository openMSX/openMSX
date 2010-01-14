// $Id$

#ifndef REVERSEMANGER_HH
#define REVERSEMANGER_HH

#include "Schedulable.hh"
#include "EventListener.hh"
#include "StateChangeListener.hh"
#include "EmuTime.hh"
#include "shared_ptr.hh"
#include <vector>
#include <map>
#include <memory>

namespace openmsx {

class MSXMotherBoard;
class EventDistributor;
class ReverseCmd;
class MemBuffer;

class ReverseManager : private Schedulable, private EventListener
                     , private StateChangeListener
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
	typedef std::vector<shared_ptr<StateChange> > Events;

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
	std::string debugInfo() const;
	void goBack(const std::vector<std::string>& tokens);
	std::string saveReplay(const std::vector<std::string>& tokens);
	std::string loadReplay(const std::vector<std::string>& tokens);
	
	void goToSnapshot(Chunks::iterator chunk_it, EmuTime::param targetTime);

	void transferHistory(ReverseHistory& oldHistory,
                             unsigned oldCollectCount,
                             unsigned oldEventCount);
	void restoreReplayLog(Events events);
	void takeSnapshot(EmuTime::param time);
	void schedule(EmuTime::param time);
	void replayNextEvent();
	template<unsigned N> void dropOldSnapshots(unsigned count);

	// Schedulable
	virtual void executeUntil(EmuTime::param time, int userData);
	virtual const std::string& schedName() const;

	// EventListener
	virtual bool signalEvent(shared_ptr<const Event> event);

	// StateChangeListener
	virtual void signalStateChange(shared_ptr<StateChange> event);
	virtual void stopReplay(EmuTime::param time);

	MSXMotherBoard& motherBoard;
	EventDistributor& eventDistributor;
	const std::auto_ptr<ReverseCmd> reverseCmd;
	ReverseHistory history;
	unsigned collectCount; // nb taken snapshots (0 = not collecting)
	unsigned replayIndex;
	bool pendingTakeSnapshot;

	friend class ReverseCmd;
	friend struct Replay;
};

} // namespace openmsx

#endif
