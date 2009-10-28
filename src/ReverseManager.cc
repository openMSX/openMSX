// $Id$

#include "ReverseManager.hh"
#include "MSXMotherBoard.hh"
#include "MSXEventDistributor.hh"
#include "InputEvents.hh"
#include "Reactor.hh"
#include "Clock.hh"
#include "Command.hh"
#include "CommandException.hh"
#include "StringOp.hh"
#include "serialize.hh"
#include <cassert>

using std::string;
using std::vector;

namespace openmsx {

class ReverseCmd : public SimpleCommand
{
public:
	ReverseCmd(ReverseManager& manager, CommandController& controller);
	virtual string execute(const vector<string>& tokens);
	virtual string help(const vector<string>& tokens) const;
	virtual void tabCompletion(vector<string>& tokens) const;
private:
	ReverseManager& manager;
};


// struct ReverseHistory

ReverseManager::ReverseHistory::ReverseHistory()
{
}

void ReverseManager::ReverseHistory::swap(ReverseHistory& other)
{
	std::swap(chunks, other.chunks);
	std::swap(events, other.events);
}

void ReverseManager::ReverseHistory::clear()
{
	Chunks().swap(chunks); // clear() and free storage capacity
	Events().swap(events); // clear() and free storage capacity
}


enum SyncType {
	NEW_SNAPSHOT,
	INPUT_EVENT
};

// class ReverseManager

ReverseManager::ReverseManager(MSXMotherBoard& motherBoard_)
	: Schedulable(motherBoard_.getScheduler())
	, motherBoard(motherBoard_)
	, reverseCmd(new ReverseCmd(*this, motherBoard.getCommandController()))
	, collectCount(0)
	, currentEventReplayIndex(0)
{
}

ReverseManager::~ReverseManager()
{
	stop();
}

string ReverseManager::start()
{
	if (!collectCount) {
		collectCount = 1;
		executeUntil(getCurrentTime(), NEW_SNAPSHOT);
		motherBoard.getMSXEventDistributor().registerEventListener(*this);
	}
	return "";
}

string ReverseManager::stop()
{
	if (collectCount) {
		// don't listen to events anymore
		motherBoard.getMSXEventDistributor().unregisterEventListener(*this);
	}
	// don't schedule new snapshot takings
	removeSyncPoint(NEW_SNAPSHOT);
	// stop any pending replay actions
	removeSyncPoint(INPUT_EVENT);
	history.clear();
	collectCount = 0;
	return "";
}

string ReverseManager::status()
{
	StringOp::Builder result;
	unsigned totalSize = 0;
	for (Chunks::const_iterator it = history.chunks.begin();
	     it != history.chunks.end(); ++it) {
		const ReverseChunk& chunk = it->second;
		result << it->first << ' '
		       << (chunk.time - EmuTime::zero).toDouble() << ' '
		       << ((chunk.time - EmuTime::zero).toDouble() / (motherBoard.getCurrentTime() - EmuTime::zero).toDouble()) * 100 << '%'
		       << " (" << chunk.savestate->getLength() << ")"
		       << " (next event index: " << chunk.nextEventIndex << ")\n";
		totalSize += chunk.savestate->getLength();
	}
	result << "total size: " << totalSize << '\n';
	return result;
}

string ReverseManager::go(const vector<string>& tokens)
{
	if (tokens.size() != 3) {
		throw SyntaxError();
	}
	unsigned n = StringOp::stringToInt(tokens[2]);
	Chunks::iterator it = history.chunks.find(n);
	if (it == history.chunks.end()) {
		throw CommandException("Out of range");
	}
	// erase all snapshots coming after the one we are going to
	Chunks::iterator it2 = it;
	history.chunks.erase(++it2, history.chunks.end());
	collectCount = n;

	Reactor& reactor = motherBoard.getReactor();
	Reactor::Board newBoard = reactor.createEmptyMotherBoard();
	MemInputArchive in(*it->second.savestate);
	in.serialize("machine", *newBoard);

	removeSyncPoint(NEW_SNAPSHOT);
	removeSyncPoint(INPUT_EVENT);
	// don't listen to events anymore
	motherBoard.getMSXEventDistributor().unregisterEventListener(*this);

	assert(collectCount);
	newBoard->getReverseManager().transferHistory(history, collectCount, it->second.nextEventIndex);
	collectCount = 0;
	assert(history.chunks.empty());

	reactor.replaceActiveBoard(newBoard); // TODO this board may not be the active board

	return "";
}

void ReverseManager::transferHistory(ReverseHistory& oldHistory,
                                     unsigned oldCollectCount,
				     unsigned nextEventHistoryIndex)
{
	motherBoard.getMSXEventDistributor().registerEventListener(*this);
	assert(!collectCount);
	assert(history.chunks.empty());
	history.swap(oldHistory);
	collectCount = oldCollectCount;
	schedule(getCurrentTime());
	
	// start replaying from event after eventHistoryIndex
	currentEventReplayIndex = nextEventHistoryIndex;
	replayNextEvent();
}

void ReverseManager::executeUntil(EmuTime::param time, int userData)
{
	switch (userData) {
	case NEW_SNAPSHOT: {
		assert(collectCount);
		dropOldSnapshots<25>(collectCount);

		MemOutputArchive out;
		out.serialize("machine", motherBoard);
		ReverseChunk& newChunk = history.chunks[collectCount];
		newChunk.time = time;
		newChunk.savestate.reset(new MemBuffer(out.stealBuffer()));
		newChunk.nextEventIndex = history.events.size();

		++collectCount;
		schedule(time);
		break;
	}
	case INPUT_EVENT:
		try {
			// deliver current event at current time
			motherBoard.getMSXEventDistributor().distributeEvent(history.events[currentEventReplayIndex].getEvent(), time);
			// set index to next event
			currentEventReplayIndex++;


		} catch (MSXException&) {
			// ignore
		}
		replayNextEvent();
		break;
	}
}

void ReverseManager::replayNextEvent()
{
	// schedule next event at its own time, if we're not done yet
	if (currentEventReplayIndex != history.events.size()) {
		setSyncPoint(history.events[currentEventReplayIndex].getTime(), INPUT_EVENT);
	}
}

void ReverseManager::signalEvent(shared_ptr<const Event> event,
		EmuTime::param time)
{
	// in replay mode, we do not record the events
	// but if the event is not one that we just replayed, we should
	// stop replaying and start recording again
	// and remove the rest if the history
	if (dynamic_cast<const TimedEvent*> (event.get())) {
		if ((currentEventReplayIndex != history.events.size()) && (event == history.events[currentEventReplayIndex].getEvent())) {
			// this is an event we just replayed, ignore
		} else {
			// if we're replaying, stop it and erase history
			if (pendingSyncPoint(INPUT_EVENT)) { // still replaying
				// stop replaying by canceling the already scheduled event
				removeSyncPoint(INPUT_EVENT);
				// erase all events after the current index
				history.events.erase(history.events.begin() + currentEventReplayIndex + 1, history.events.end());
			}
			// record event (TimedEvent only)
			history.events.push_back(EventChunk(time, event));
		}
	}
}

// Should be called each time a new snapshot is added.
// This function will erase zero or more earlier snapshots so that there are
// more snapshots of recent history and less of distant history. It has the
// following properties:
//  - the very oldest snapshot is never deleted
//  - it keeps the N or N+1 most recent snapshots (snapshot distance = 1)
//  - then it keeps N or N+1 with snapshot distance 2
//  - then N or N+1 with snapshot distance 4
//  - ... and so on
template<unsigned N>
void ReverseManager::dropOldSnapshots(unsigned count)
{
	unsigned y = (count + N - 1) ^ (count + N);
	unsigned d = N;
	unsigned d2 = 2 * N + 1;
	while (true) {
		y >>= 1;
		if ((y == 0) || (count <= d)) return;
		history.chunks.erase(count - d);
		d += d2;
		d2 *= 2;
	}
}

void ReverseManager::schedule(EmuTime::param time)
{
	Clock<1> clock(time);
	clock += 1;
	setSyncPoint(clock.getTime(), NEW_SNAPSHOT);
}

const string& ReverseManager::schedName() const
{
	static const string NAME = "ReverseManager";
	return NAME;
}


// class ReverseCmd

ReverseCmd::ReverseCmd(ReverseManager& manager_, CommandController& controller)
	: SimpleCommand(controller, "reverse")
	, manager(manager_)
{
}

string ReverseCmd::execute(const vector<string>& tokens)
{
	if (tokens.size() < 2) {
		throw CommandException("Missing subcommand");
	}
	if (tokens[1] == "start") {
		return manager.start();
	} else if (tokens[1] == "stop") {
		return manager.stop();
	} else if (tokens[1] == "status") {
		return manager.status();
	} else if (tokens[1] == "go") {
		return manager.go(tokens);
	} else {
		throw CommandException("Invalid subcommand");
	}
}

string ReverseCmd::help(const vector<string>& /*tokens*/) const
{
	return "!! this is NOT the final command, this is only for experiments !!\n"
	       "start   start collecting reverse data\n"
	       "stop    stop  collecting\n"
	       "status  give overview of collected data\n"
	       "go <n>  go back to a previously collected point\n";
}

void ReverseCmd::tabCompletion(vector<string>& /*tokens*/) const
{
	// TODO
}

} // namespace openmsx
