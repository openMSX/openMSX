// $Id$

#include "ReverseManager.hh"
#include "MSXMotherBoard.hh"
#include "EventDistributor.hh"
#include "StateChangeDistributor.hh"
#include "XMLException.hh"
#include "XMLElement.hh"
#include "TclObject.hh"
#include "FileOperations.hh"
#include "FileContext.hh"
#include "StateChange.hh"
#include "Reactor.hh"
#include "Clock.hh"
#include "Command.hh"
#include "CommandException.hh"
#include "StringOp.hh"
#include "serialize.hh"
#include "serialize_stl.hh"
#include <cassert>

using std::string;
using std::vector;
using std::set;

namespace openmsx {

// A replay is a struct that contains a motherboard and an MSX event
// log. Those combined are a replay, because you can replay the events
// from an existing motherboard state.
struct Replay
{
	ReverseManager::Events* events;
	Reactor::Board motherBoard;

	template<typename Archive>
	void serialize(Archive& ar, unsigned /*version*/)
	{
		ar.serialize("snapshot", *motherBoard);
		ar.serialize("events", *events);
	}
};

class ReverseCmd : public Command
{
public:
	ReverseCmd(ReverseManager& manager, CommandController& controller);
	virtual void execute(const vector<TclObject*>& tokens, TclObject& result);
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


class EndLogEvent : public StateChange
{
public:
	EndLogEvent() {} // for serialize
	EndLogEvent(EmuTime::param time)
		: StateChange(time)
	{
	}

	template<typename Archive> void serialize(Archive& ar, unsigned /*version*/)
	{
		ar.template serializeBase<StateChange>(*this);
	}
};
REGISTER_POLYMORPHIC_CLASS(StateChange, EndLogEvent, "EndLog");

// class ReverseManager

enum SyncType {
	NEW_SNAPSHOT,
	INPUT_EVENT
};

ReverseManager::ReverseManager(MSXMotherBoard& motherBoard_)
	: Schedulable(motherBoard_.getScheduler())
	, motherBoard(motherBoard_)
	, eventDistributor(motherBoard.getReactor().getEventDistributor())
	, reverseCmd(new ReverseCmd(*this, motherBoard.getCommandController()))
	, collectCount(0)
	, replayIndex(0)
	, pendingTakeSnapshot(false)
{
	eventDistributor.registerEventListener(OPENMSX_TAKE_REVERSE_SNAPSHOT, *this);

	assert(!collecting());
	assert(!replaying());
}

ReverseManager::~ReverseManager()
{
	stop();

	eventDistributor.unregisterEventListener(OPENMSX_TAKE_REVERSE_SNAPSHOT, *this);
}

bool ReverseManager::collecting() const
{
	return collectCount != 0;
}

bool ReverseManager::replaying() const
{
	return replayIndex != history.events.size();
}

void ReverseManager::start()
{
	if (!collecting()) {
		// create first snapshot
		assert(collectCount == 0);
		takeSnapshot(getCurrentTime());
		assert(collectCount == 1);
		// start recording events
		motherBoard.getStateChangeDistributor().registerRecorder(*this);
	}
	assert(collecting());
}

void ReverseManager::stop()
{
	if (collecting()) {
		motherBoard.getStateChangeDistributor().unregisterRecorder(*this);
		removeSyncPoint(NEW_SNAPSHOT); // don't schedule new snapshot takings
		removeSyncPoint(INPUT_EVENT); // stop any pending replay actions
		history.clear();
		collectCount = 0;
		replayIndex = 0;
		pendingTakeSnapshot = false;
	}
	assert(!pendingTakeSnapshot);
	assert(!collecting());
	assert(!replaying());
}

void ReverseManager::debugInfo(TclObject& result) const
{
	// TODO this is useful during development, but for the end user this
	// information means nothing. We should remove this later.
	StringOp::Builder res;
	unsigned totalSize = 0;
	for (Chunks::const_iterator it = history.chunks.begin();
	     it != history.chunks.end(); ++it) {
		const ReverseChunk& chunk = it->second;
		res << it->first << ' '
		    << (chunk.time - EmuTime::zero).toDouble() << ' '
		    << ((chunk.time - EmuTime::zero).toDouble() / (motherBoard.getCurrentTime() - EmuTime::zero).toDouble()) * 100 << '%'
		    << " (" << chunk.savestate->getLength() << ")"
		    << " (next event index: " << chunk.eventCount << ")\n";
		totalSize += chunk.savestate->getLength();
	}
	res << "total size: " << totalSize << '\n';
	result.setString(res);
}

void ReverseManager::goBack(const vector<TclObject*>& tokens)
{
	if (history.chunks.empty())
		throw CommandException("No recording...");
	if (tokens.size() != 3) {
		throw SyntaxError();
	}
	double t = tokens[2]->getDouble();

	// find oldest snapshot that is not newer than requested time
	EmuTime targetTime = EmuTime::zero;
	Chunks::iterator it = history.chunks.begin();
	if (EmuDuration(t) <= (getCurrentTime() - it->second.time)) {
		targetTime = getCurrentTime() - EmuDuration(t);
		// TODO ATM we do a linear search, could be improved to do a
		//      binary search.
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
		targetTime = it->second.time;
	}
	goToSnapshot(it, targetTime);
}

void ReverseManager::saveReplay(const vector<TclObject*>& tokens, TclObject& result)
{
	if (history.chunks.empty()) {
		throw CommandException("No recording...");
	}

	string fileName;
	if (tokens.size() == 2) {
		fileName = FileOperations::getNextNumberedFileName(
		                "replays", "openmsx", ".gz");
	} else if (tokens.size() == 3) {
		fileName = tokens[2]->getString();
	} else {
		throw SyntaxError();
	}

	// restore first snapshot to be able to serialize it to a file
	Reactor& reactor = motherBoard.getReactor();
	Reactor::Board newBoard = reactor.createEmptyMotherBoard();
	MemInputArchive in(*history.chunks.begin()->second.savestate);
	in.serialize("machine", *newBoard);

	// add sentinel when there isn't one yet
	bool addSentinel = history.events.empty() ||
		!dynamic_cast<EndLogEvent*>(history.events.back().get());
	if (addSentinel) {
		/// make sure the replay log ends with a EndLogEvent
		history.events.push_back(shared_ptr<StateChange>(
			new EndLogEvent(getCurrentTime())));
	}

	XmlOutputArchive out(fileName);
	Replay replay;
	replay.events = &history.events;
	replay.motherBoard = newBoard;
	out.serialize("replay", replay);

	if (addSentinel) {
		// Is there a cleaner way to only add the sentinel in the log?
		// I mean avoid changing/restoring the current log. We could
		// make a copy and work on that, but that seems much less
		// efficient.
		history.events.pop_back();
	}

	result.setString("Saved replay to " + fileName);
}

void ReverseManager::loadReplay(const vector<TclObject*>& tokens, TclObject& result)
{
	if (tokens.size() != 3) {
		throw SyntaxError();
	}

	UserDataFileContext context("replays");
	string fileName = context.resolve(motherBoard.getCommandController(),
	                                  tokens[2]->getString());

	// restore replay
	Replay replay;
	Events events;
	Reactor& reactor = motherBoard.getReactor();
	replay.motherBoard = reactor.createEmptyMotherBoard();
	replay.events = &events;
	try {
		XmlInputArchive in(fileName);
		in.serialize("replay", replay);
	} catch (XMLException& e) {
		throw CommandException("Cannot load replay, bad file format: " + e.getMessage());
	} catch (MSXException& e) {
		throw CommandException("Cannot load replay: " + e.getMessage());
	}

	// load was successful, only start changing current
	// ReverseManager/MSXMotherBoard from here on

	// if we are also collecting, better stop that now
	stop();
	
	// put the events in the new MSXMotherBoard, also an initial in-memory
	// snapshot must be created and maybe more to bring the new
	// ReverseManager to a valid state (with replay info)
	replay.motherBoard->getReverseManager().restoreReplayLog(events);

	// switch to the new MSXMotherBoard
	// TODO this is not correct if this board was not the active board
	reactor.replaceActiveBoard(replay.motherBoard);

	result.setString("Loaded replay from " + fileName);
}

/** Go to snapshot given by iterator and targetTime
 */
void ReverseManager::goToSnapshot(Chunks::iterator it, EmuTime::param targetTime)
{
	if (!replaying()) {
		// terminate replay log with EndLogEvent
		history.events.push_back(shared_ptr<StateChange>(
			new EndLogEvent(getCurrentTime())));
		++replayIndex;
	}
	// replay-log must end with EndLogEvent, either we just added it, or
	// it was there already
	assert(!history.events.empty());
	assert(dynamic_cast<const EndLogEvent*>(history.events.back().get()));

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

	// fast forward to the required time
	newBoard->fastForward(targetTime);

	// switch to the new MSXMotherBoard
	// TODO this is not correct if this board was not the active board
	reactor.replaceActiveBoard(newBoard);

	assert(!collecting());
	assert(newBoard->getReverseManager().collecting());
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
	motherBoard.getStateChangeDistributor().registerRecorder(*this);
	assert(collecting());

	// start replaying events
	replayIndex = oldEventCount;
	// replay log contains at least the EndLogEvent
	assert(replayIndex < history.events.size());
	replayNextEvent();
}

void ReverseManager::restoreReplayLog(Events events)
{
	assert(!collecting());
	start(); // creates initial in-memory snapshot

	// steal event-data from caller
	// also very efficient because it avoids a copy
	swap(history.events, events);

	assert(replayIndex == 0);
	assert(!history.events.empty());
	assert(dynamic_cast<EndLogEvent*>(history.events.back().get()));
	replayNextEvent();
	assert(replaying());
}

void ReverseManager::executeUntil(EmuTime::param /*time*/, int userData)
{
	switch (userData) {
	case NEW_SNAPSHOT:
		// During record we should take regular snapshots, and 'now'
		// it's been a while since the last snapshot. But 'now' can be
		// in the middle of a CPU instruction (1). However the CPU
		// emulation code cannot handle taking snapshots at arbitrary
		// moments in EmuTime (2)(3)(4). So instead we send out an
		// event that indicates we want to take a snapshot (5).
		// (1) Schedulables are executed at the exact requested
		//     EmuTime, even in the middle of a Z80 instruction.
		// (2) The CPU code serializes all registers, current time and
		//     various other status info, but not enough info to be
		//     able to resume in the middle of an instruction.
		// (3) Only the CPU has this limitation of not being able to
		//     take a snapshot at any EmuTime, all other devices can.
		//     This is because in our emulation model the CPU 'drives
		//     time forward'. It's the only device code that can be
		//     interrupted by other emulation code (via Schedulables).
		// (4) In the past we had a CPU core that could execute/resume
		//     partial instructions (search SVN history). Though it was
		//     much more complex and it also ran slower than the
		//     current code.
		// (5) Events are delivered from the Reactor code. That code
		//     only runs when the CPU code has exited (meaning no
		//     longer active in any stackframe). So it's executed right
		//     after the CPU has finished the current instruction. And
		//     that's OK, we only require regular snapshots here, they
		//     should not be *exactly* equally far apart in time.
		pendingTakeSnapshot = true;
		eventDistributor.distributeEvent(
			new SimpleEvent(OPENMSX_TAKE_REVERSE_SNAPSHOT));
		break;
	case INPUT_EVENT:
		shared_ptr<StateChange> event = history.events[replayIndex];
		try {
			// deliver current event at current time
			motherBoard.getStateChangeDistributor().distributeReplay(event);
		} catch (MSXException&) {
			// can throw in case we replay a command that fails
			// ignore
		}
		if (!dynamic_cast<const EndLogEvent*>(event.get())) {
			++replayIndex;
			replayNextEvent();
		} else {
			assert(!replaying()); // stopped by replay of EndLogEvent
		}
		break;
	}
}

bool ReverseManager::signalEvent(shared_ptr<const Event> event)
{
	(void)event;
	assert(event->getType() == OPENMSX_TAKE_REVERSE_SNAPSHOT);

	// This event is send to all MSX machines, make sure it's actually this
	// machine that requested the snapshot.
	if (pendingTakeSnapshot) {
		pendingTakeSnapshot = false;
		takeSnapshot(getCurrentTime());
	}
	return true;
}

void ReverseManager::takeSnapshot(EmuTime::param time)
{
	++collectCount;
	dropOldSnapshots<25>(collectCount);

	MemOutputArchive out;
	out.serialize("machine", motherBoard);
	ReverseChunk& newChunk = history.chunks[collectCount];
	newChunk.time = time;
	newChunk.savestate.reset(new MemBuffer(out.stealBuffer()));
	newChunk.eventCount = replayIndex;

	schedule(getCurrentTime());
}

void ReverseManager::replayNextEvent()
{
	// schedule next event at its own time
	assert(replayIndex < history.events.size());
	setSyncPoint(history.events[replayIndex]->getTime(), INPUT_EVENT);
}

void ReverseManager::signalStateChange(shared_ptr<StateChange> event)
{
	if (replaying()) {
		// this is an event we just replayed
		assert(event == history.events[replayIndex]);
		if (dynamic_cast<EndLogEvent*>(event.get())) {
			motherBoard.getStateChangeDistributor().stopReplay(
				event->getTime());
		} else {
			// ignore all other events
		}
	} else {
		// record event
		history.events.push_back(event);
		++replayIndex;
		assert(!replaying());
	}
}

void ReverseManager::stopReplay(EmuTime::param /*time*/)
{
	if (replaying()) {
		// if we're replaying, stop it and erase remainder of event log
		removeSyncPoint(INPUT_EVENT);
		Events& events = history.events;
		events.erase(events.begin() + replayIndex, events.end());
	}
	assert(!replaying());
}

/* Should be called each time a new snapshot is added.
 * This function will erase zero or more earlier snapshots so that there are
 * more snapshots of recent history and less of distant history. It has the
 * following properties:
 *  - the very oldest snapshot is never deleted
 *  - it keeps the N or N+1 most recent snapshots (snapshot distance = 1)
 *  - then it keeps N or N+1 with snapshot distance 2
 *  - then N or N+1 with snapshot distance 4
 *  - ... and so on
 * @param count The index of the just added (or about to be added) element.
 *              First element should have index 1.
 */
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
	: Command(controller, "reverse")
	, manager(manager_)
{
}

void ReverseCmd::execute(const vector<TclObject*>& tokens, TclObject& result)
{
	if (tokens.size() < 2) {
		throw CommandException("Missing subcommand");
	}
	string subcommand = tokens[1]->getString();
	if        (subcommand == "start") {
		manager.start();
	} else if (subcommand == "stop") {
		manager.stop();
	} else if (subcommand == "debug") {
		manager.debugInfo(result);
	} else if (subcommand == "goback") {
		manager.goBack(tokens);
	} else if (subcommand == "savereplay") {
		return manager.saveReplay(tokens, result);
	} else if (subcommand == "loadreplay") {
		return manager.loadReplay(tokens, result);
	} else {
		throw CommandException("Invalid subcommand: " + subcommand);
	}
}

string ReverseCmd::help(const vector<string>& /*tokens*/) const
{
	return "!! this is NOT the final command, this is only for experiments !!\n"
	       "start               start collecting reverse data\n"
	       "stop                stop collecting\n"
	       //"status              \n"
	       "goback <n>          go back <n> seconds in time (for now: approx!)\n"
	       "savereplay [<name>] save the first snapshot and all replay data as a 'replay' (with optional name)\n"
	       "loadreplay <name>   load a replay (snapshot and replay data) with given name and start replaying\n";
}

void ReverseCmd::tabCompletion(vector<string>& tokens) const
{
	if (tokens.size() == 2) {
		set<string> subCommands;
		subCommands.insert("start");
		subCommands.insert("stop");
		//subCommands.insert("status");
		subCommands.insert("goback");
		subCommands.insert("savereplay");
		subCommands.insert("loadreplay");
		completeString(tokens, subCommands);
	} else if (tokens.size() == 3) {
		if (tokens[1] == "loadreplay") {
			UserDataFileContext context("replays");
			completeFileName(getCommandController(), tokens, context);
		}
	}
}

} // namespace openmsx
