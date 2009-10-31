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

void ReverseManager::ReverseHistory::swap(ReverseHistory& other)
{
	std::swap(chunks, other.chunks);
	std::swap(events, other.events);
}

void ReverseManager::ReverseHistory::clear()
{
	// clear() and free storage capacity
	Chunks().swap(chunks);
	Events().swap(events);
}


// class ReverseManager

enum SyncType {
	NEW_SNAPSHOT,
	INPUT_EVENT
};

ReverseManager::ReverseManager(MSXMotherBoard& motherBoard_)
	: Schedulable(motherBoard_.getScheduler())
	, motherBoard(motherBoard_)
	, reverseCmd(new ReverseCmd(*this, motherBoard.getCommandController()))
	, collectCount(0)
	, replayIndex(0)
{
	assert(!collecting());
	assert(!replaying());
}

ReverseManager::~ReverseManager()
{
	stop();
}

bool ReverseManager::collecting() const
{
	return collectCount;
}

bool ReverseManager::replaying() const
{
	return replayIndex != history.events.size();
}

void ReverseManager::start()
{
	if (!collecting()) {
		// create first snapshot
		collectCount = 1;
		executeUntil(getCurrentTime(), NEW_SNAPSHOT);
		// start recording events
		motherBoard.getMSXEventDistributor().registerEventListener(*this);
	}
	assert(collecting());
}

void ReverseManager::stop()
{
	if (collecting()) {
		motherBoard.getMSXEventDistributor().unregisterEventListener(*this);
		removeSyncPoint(NEW_SNAPSHOT); // don't schedule new snapshot takings
		removeSyncPoint(INPUT_EVENT); // stop any pending replay actions
		history.clear();
		collectCount = 0;
		replayIndex = 0;
	}
	assert(!collecting());
	assert(!replaying());
}

string ReverseManager::status()
{
	// TODO this is useful during development, but for the end user this
	// information means nothing. We should remove this later.
	StringOp::Builder result;
	unsigned totalSize = 0;
	for (Chunks::const_iterator it = history.chunks.begin();
	     it != history.chunks.end(); ++it) {
		const ReverseChunk& chunk = it->second;
		result << it->first << ' '
		       << (chunk.time - EmuTime::zero).toDouble() << ' '
		       << ((chunk.time - EmuTime::zero).toDouble() / (motherBoard.getCurrentTime() - EmuTime::zero).toDouble()) * 100 << '%'
		       << " (" << chunk.savestate->getLength() << ")"
		       << " (next event index: " << chunk.eventCount << ")\n";
		totalSize += chunk.savestate->getLength();
	}
	result << "total size: " << totalSize << '\n';
	return result;
}

void ReverseManager::go(const vector<string>& tokens)
{
	// TODO useful during development, but should probably be removed later
	if (tokens.size() != 3) {
		throw SyntaxError();
	}
	unsigned n = StringOp::stringToInt(tokens[2]);
	Chunks::iterator it = history.chunks.find(n);
	if (it == history.chunks.end()) {
		throw CommandException("Out of range");
	}
	goToSnapshot(it);
}

void ReverseManager::goBack(const vector<string>& tokens)
{
	if (history.chunks.empty())
		throw CommandException("No recording...");
	if (tokens.size() != 3) {
		throw SyntaxError();
	}
	double t = StringOp::stringToDouble(tokens[2]);

	// find oldest snapshot that is not newer than requested time
	Chunks::iterator it = history.chunks.begin();
	if (EmuDuration(t) <= (getCurrentTime() - it->second.time)) {
		// TODO ATM we do a linear search, could be improved to do a
		//      binary search.
		EmuTime targetTime = getCurrentTime() - EmuDuration(t);
		assert(it->second.time <= targetTime); // first one is not newer
		assert(it != history.chunks.end()); // there are snapshots
		do {
			++it;
		} while (it != history.chunks.end() &&
		         it->second.time <= targetTime);
		// We found the first one that's newer, previous one is last
		// one that's not newer (thus older or equal).
		assert(it != history.chunks.begin());
		--it;
		assert(it->second.time <= targetTime);
	} else {
		// Requested time is before first snapshot. We can't go back
		// further than first snapshot, so take that one.
	}
	goToSnapshot(it);
}

void ReverseManager::goToSnapshot(Chunks::iterator it)
{
	// erase all snapshots coming after the one we are going to
	assert(it != history.chunks.end());
	Chunks::iterator it2 = it;
	history.chunks.erase(++it2, history.chunks.end());

	// restore old snapshot
	Reactor& reactor = motherBoard.getReactor();
	Reactor::Board newBoard = reactor.createEmptyMotherBoard();
	MemInputArchive in(*it->second.savestate);
	in.serialize("machine", *newBoard);

	// Transfer history from this ReverseManager to the one in the new
	// MSXMotherBoard. Also we should stop collecting in this ReverseManager,
	// and start collecting in the new one.
	assert(collecting());
	newBoard->getReverseManager().transferHistory(
		history, it->first, it->second.eventCount);
	stop();

	// switch to the new MSXMotherBoard
	// TODO this is not correct if this board was not the active board
	reactor.replaceActiveBoard(newBoard);
}

void ReverseManager::transferHistory(ReverseHistory& oldHistory,
                                     unsigned oldCollectCount,
                                     unsigned oldEventCount)
{
	assert(!collecting());
	assert(history.chunks.empty());

	// actual history transfer
	history.swap(oldHistory);

	// resume collecting (and event recording)
	collectCount = oldCollectCount;
	schedule(getCurrentTime());
	motherBoard.getMSXEventDistributor().registerEventListener(*this);
	assert(collecting());

	// start replaying events
	replayIndex = oldEventCount;
	replayNextEvent();
	// It's possible there were no events to replay, so we may not actually
	// be replaying at this point.
}

void ReverseManager::executeUntil(EmuTime::param time, int userData)
{
	switch (userData) {
	case NEW_SNAPSHOT: {
		assert(collecting());
		dropOldSnapshots<25>(collectCount);

		MemOutputArchive out;
		out.serialize("machine", motherBoard);
		ReverseChunk& newChunk = history.chunks[collectCount];
		newChunk.time = time;
		newChunk.savestate.reset(new MemBuffer(out.stealBuffer()));
		newChunk.eventCount = replayIndex;

		++collectCount;
		schedule(time);
		break;
	}
	case INPUT_EVENT:
		// deliver current event at current time
		motherBoard.getMSXEventDistributor().distributeEvent(
			history.events[replayIndex].event, time);
		++replayIndex;
		replayNextEvent();
		break;
	}
}

void ReverseManager::replayNextEvent()
{
	// schedule next event at its own time, if we're not done yet
	if (replayIndex != history.events.size()) {
		setSyncPoint(history.events[replayIndex].time,
		             INPUT_EVENT);
	}
}

void ReverseManager::signalEvent(shared_ptr<const Event> event,
                                 EmuTime::param time)
{
	if (!dynamic_cast<const TimedEvent*>(event.get())) {
		// TODO this is wrong:
		//   for example MSXCommandEvents should also be recorded
		return;
	}

	Events& events = history.events;
	if (replaying() && (event == events[replayIndex].event)) {
		// this is an event we just replayed, ignore it
	} else {
		if (replaying()) {
			// if we're replaying, stop it and erase remainder of event log
			removeSyncPoint(INPUT_EVENT);
			events.erase(events.begin() + replayIndex, events.end());
			assert(!replaying());
		}
		// record event
		events.push_back(EventChunk(time, event));
		++replayIndex;
		assert(!replaying());
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
		manager.start();
	} else if (tokens[1] == "stop") {
		manager.stop();
	} else if (tokens[1] == "status") {
		return manager.status();
	} else if (tokens[1] == "goback") {
		manager.goBack(tokens);
	} else if (tokens[1] == "go") {
		manager.go(tokens);
	} else {
		throw CommandException("Invalid subcommand");
	}
	return "";
}

string ReverseCmd::help(const vector<string>& /*tokens*/) const
{
	return "!! this is NOT the final command, this is only for experiments !!\n"
	       "start      start collecting reverse data\n"
	       "stop       stop  collecting\n"
	       "status     give overview of collected data\n"
	       "go <n>     go to a previously collected point\n"
	       "goback <n> go back <n> seconds in time (for now: approx!)\n";
}

void ReverseCmd::tabCompletion(vector<string>& /*tokens*/) const
{
	// TODO
}

} // namespace openmsx
