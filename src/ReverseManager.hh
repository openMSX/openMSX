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
class Keyboard;
class EventDelay;
class EventDistributor;
class ReverseCmd;
class MemBuffer;
class TclObject;

class ReverseManager : private Schedulable, private EventListener
                     , private StateChangeListener
{
public:
	ReverseManager(MSXMotherBoard& motherBoard);
	~ReverseManager();

	// Keyboard is special because we need to transfer the host keyboard
	// state on 'reverse goto' to be able to resynchronize when replay
	// stops. See Keyboard::transferHostKeyMatrix() for more info.
	void registerKeyboard(Keyboard& keyboard);

	// To not loose any events we need to flush delayed events before
	// switching machine. See comments in goTo() for more info.
	void registerEventDelay(EventDelay& eventDelay);

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

	bool isCollecting() const;
	bool isReplaying() const;

	void start();
	void stop();
	void status(TclObject& result) const;
	void debugInfo(TclObject& result) const;
	void goBack(const std::vector<TclObject*>& tokens);
	void goTo(const std::vector<TclObject*>& tokens);
	void saveReplay(const std::vector<TclObject*>& tokens, TclObject& result);
	void loadReplay(const std::vector<TclObject*>& tokens, TclObject& result);
	
	EmuTime::param getEndTime() const;
	void goTo(EmuTime::param targetTime);
	void transferHistory(ReverseHistory& oldHistory,
                             unsigned oldEventCount);
	void restoreSnapshots(const std::vector<ReverseChunk>& chunks);
	void restoreReplayLog(Events events);
	unsigned getNextSeqNum(EmuTime::param time);
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
	Keyboard* keyboard;
	EventDelay* eventDelay;
	ReverseHistory history;
	unsigned replayIndex;
	bool collecting;
	bool pendingTakeSnapshot;

	friend class ReverseCmd;
	friend struct Replay;
};

} // namespace openmsx

#endif
