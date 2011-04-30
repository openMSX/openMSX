// $Id$

#include "ReverseManager.hh"
#include "MSXMotherBoard.hh"
#include "EventDistributor.hh"
#include "StateChangeDistributor.hh"
#include "Keyboard.hh"
#include "Debugger.hh"
#include "EventDelay.hh"
#include "MSXMixer.hh"
#include "XMLException.hh"
#include "XMLElement.hh"
#include "TclObject.hh"
#include "FileOperations.hh"
#include "FileContext.hh"
#include "StateChange.hh"
#include "Reactor.hh"
#include "Command.hh"
#include "CommandException.hh"
#include "MemBuffer.hh"
#include "StringOp.hh"
#include "serialize.hh"
#include "serialize_stl.hh"
#include "checked_cast.hh"
#include "ref.hh"
#include <cassert>

using std::string;
using std::vector;
using std::set;

namespace openmsx {

// Time between two snapshots (in seconds)
static const double SNAPSHOT_PERIOD = 1.0;

// Max number of snapshots in a replay
static const unsigned MAX_NOF_SNAPSHOTS = 10;

// Min distance between snapshots in replay (in seconds)
static const EmuDuration MIN_PARTITION_LENGTH = EmuDuration(60.0);

static const string REPLAY_DIR = "replays";

// A replay is a struct that contains a vector of motherboards and an MSX event
// log. Those combined are a replay, because you can replay the events from an
// existing motherboard state: the vector has to have at least one motherboard
// (the initial state), but can have optionally more motherboards, which are
// merely in-between snapshots, so it is quicker to jump to a later time in the
// event log.

typedef std::vector<Reactor::Board> MotherBoards;

struct Replay
{
	Replay(Reactor& reactor_)
		: reactor(reactor_), currentTime(EmuTime::dummy()) {}

	Reactor& reactor;

	ReverseManager::Events* events;
	MotherBoards motherBoards;
	EmuTime currentTime;
	// this is the amount of times the reverse goto command was used, which
	// is interesting for the TAS community (see tasvideos.org). It's an
	// indication of the effort it took to create the replay. Note that
	// there is no way to verify this number.
	unsigned reRecordCount;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version)
	{
		if (ar.versionAtLeast(version, 2)) {
			ar.serializeWithID("snapshots", motherBoards, ref(reactor));
		} else {
			Reactor::Board newBoard = reactor.createEmptyMotherBoard();
			ar.serialize("snapshot", *newBoard);
			motherBoards.push_back(newBoard);
		}

		ar.serialize("events", *events);

		if (ar.versionAtLeast(version, 3)) {
			ar.serialize("currentTime", currentTime);
		} else {
			assert(ar.isLoader());
			assert(!events->empty());
			currentTime = events->back()->getTime();
		}

		if (ar.versionAtLeast(version, 4)) {
			ar.serialize("reRecordCount", reRecordCount);
		}
	}
};
SERIALIZE_CLASS_VERSION(Replay, 4);

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


// struct ReverseChunk

ReverseManager::ReverseChunk::ReverseChunk()
	: time(EmuTime::zero)
{
}

ReverseManager::ReverseChunk::~ReverseChunk()
{
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
	, keyboard(0)
	, eventDelay(0)
	, replayIndex(0)
	, collecting(false)
	, pendingTakeSnapshot(false)
	, reRecordCount(0)
{
	eventDistributor.registerEventListener(OPENMSX_TAKE_REVERSE_SNAPSHOT, *this);

	assert(!isCollecting());
	assert(!isReplaying());
}

ReverseManager::~ReverseManager()
{
	stop();

	eventDistributor.unregisterEventListener(OPENMSX_TAKE_REVERSE_SNAPSHOT, *this);
}

void ReverseManager::registerKeyboard(Keyboard& keyboard_)
{
	keyboard = &keyboard_;
}
void ReverseManager::registerEventDelay(EventDelay& eventDelay_)
{
	eventDelay = &eventDelay_;
}

void ReverseManager::setReRecordCount(unsigned reRecordCount_)
{
	reRecordCount = reRecordCount_;
}

bool ReverseManager::isCollecting() const
{
	return collecting;
}

bool ReverseManager::isReplaying() const
{
	return replayIndex != history.events.size();
}

void ReverseManager::start()
{
	if (!isCollecting()) {
		// create first snapshot
		collecting = true;
		takeSnapshot(getCurrentTime());
		// start recording events
		motherBoard.getStateChangeDistributor().registerRecorder(*this);
	}
	assert(isCollecting());
}

void ReverseManager::stop()
{
	if (isCollecting()) {
		motherBoard.getStateChangeDistributor().unregisterRecorder(*this);
		removeSyncPoint(NEW_SNAPSHOT); // don't schedule new snapshot takings
		removeSyncPoint(INPUT_EVENT); // stop any pending replay actions
		history.clear();
		replayIndex = 0;
		collecting = false;
		pendingTakeSnapshot = false;
	}
	assert(!pendingTakeSnapshot);
	assert(!isCollecting());
	assert(!isReplaying());
}

EmuTime::param ReverseManager::getEndTime(const ReverseHistory& history) const
{
	if (!history.events.empty()) {
		if (const EndLogEvent* ev = dynamic_cast<const EndLogEvent*>(
				history.events.back().get())) {
			// last log element is EndLogEvent, use that
			return ev->getTime();
		}
	}
	// otherwise use current time
	assert(!isReplaying());
	return getCurrentTime();
}

void ReverseManager::status(TclObject& result) const
{
	result.addListElement("status");
	if (!isCollecting()) {
		result.addListElement("disabled");
	} else if (isReplaying()) {
		result.addListElement("replaying");
	} else {
		result.addListElement("enabled");
	}

	result.addListElement("begin");
	EmuTime begin(isCollecting() ? history.chunks.begin()->second.time
	                           : EmuTime::zero);
	result.addListElement((begin - EmuTime::zero).toDouble());

	result.addListElement("end");
	EmuTime end(isCollecting() ? getEndTime(history) : EmuTime::zero);
	result.addListElement((end - EmuTime::zero).toDouble());

	result.addListElement("current");
	EmuTime current(isCollecting() ? getCurrentTime() : EmuTime::zero);
	result.addListElement((current - EmuTime::zero).toDouble());

	result.addListElement("snapshots");
	TclObject snapshots;
	for (Chunks::const_iterator it = history.chunks.begin();
	     it != history.chunks.end(); ++it) {
		EmuTime time = it->second.time;
		snapshots.addListElement((time - EmuTime::zero).toDouble());
	}
	result.addListElement(snapshots);
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
		    << " (" << chunk.savestate->size() << ')'
		    << " (next event index: " << chunk.eventCount << ")\n";
		totalSize += chunk.savestate->size();
	}
	res << "total size: " << totalSize << '\n';
	result.setString(res);
}

void ReverseManager::goBack(const vector<TclObject*>& tokens)
{
	if (tokens.size() != 3) {
		throw SyntaxError();
	}
	double t = tokens[2]->getDouble();
	EmuTime now = getCurrentTime();
	EmuTime target(EmuTime::dummy());
	if (t >= 0) {
		EmuDuration d(t);
		if (d < (now - EmuTime::zero)) {
			target = now - d;
		} else {
			target = EmuTime::zero;
		}
	} else {
		target = now + EmuDuration(-t);
	}
	goTo(target);
}

void ReverseManager::goTo(const std::vector<TclObject*>& tokens)
{
	if (tokens.size() != 3) {
		throw SyntaxError();
	}
	goTo(EmuTime::zero + EmuDuration(tokens[2]->getDouble()));
}

void ReverseManager::goTo(EmuTime::param target)
{
	if (!isCollecting()) {
		throw CommandException(
			"Reverse was not enabled. First execute the 'reverse "
			"start' command to start collecting data.");
	}
	goTo(target, history); // move in current time-line
}

void ReverseManager::goTo(EmuTime::param target, ReverseHistory& history)
{
	MSXMixer& mixer = motherBoard.getMSXMixer();
	try {
		// The call to MSXMotherBoard::fastForward() below may take
		// some time to execute. The DirectX sound driver has a problem
		// (not easily fixable) that it keeps on looping the sound
		// buffer on buffer underruns (the SDL driver plays silence on
		// underrun). At the end of this function we will switch to a
		// different active MSXMotherBoard. So we can as well now
		// already mute the current MSXMotherBoard.
		mixer.mute();

		// -- Locate destination snapshot --
		// We can't go back further in the past than the first snapshot.
		assert(!history.chunks.empty());
		Chunks::iterator it = history.chunks.begin();
		EmuTime firstTime = it->second.time;
		EmuTime targetTime = std::max(target, firstTime);
		// Also don't go further into the future than 'end time'.
		targetTime = std::min(targetTime, getEndTime(history));

		// Duration of 2 PAL frames. Possible improvement is to use the
		// actual refresh rate (PAL/NTSC). But it should be the refresh
		// rate of the active video chip (v99x8/v9990) at the target
		// time. This is quite complex to get and the difference between
		// 2 PAL and 2 NTSC frames isn't that big.
		EmuDuration dur2frames(2.0 * (313.0 * 1368.0) / (3579545.0 * 6.0));
		EmuTime preTarget = ((targetTime - firstTime) > dur2frames)
		                  ? targetTime - dur2frames
		                  : firstTime;

		// find oldest snapshot that is not newer than requested time
		// TODO ATM we do a linear search, could be improved to do a binary search.
		assert(it->second.time <= preTarget); // first one is not newer
		assert(it != history.chunks.end()); // there are snapshots
		do {
			++it;
		} while (it != history.chunks.end() &&
			 it->second.time <= preTarget);
		// We found the first one that's newer, previous one is last
		// one that's not newer (thus older or equal).
		assert(it != history.chunks.begin());
		--it;
		assert(it->second.time <= preTarget);

		// Note: we don't (anymore) erase future snapshots

		// -- restore old snapshot --
		Reactor& reactor = motherBoard.getReactor();
		Reactor::Board newBoard = reactor.createEmptyMotherBoard();
		MemInputArchive in(it->second.savestate->data(),
		                   it->second.savestate->size());
		in.serialize("machine", *newBoard);

		if (eventDelay) {
			// Handle all events that are scheduled, but not yet
			// distributed. This makes sure no events get lost
			// (important to keep host/msx keyboard in sync).
			eventDelay->flush();
		}

		// terminate replay log with EndLogEvent (if not there already)
		if (history.events.empty() ||
		    !dynamic_cast<const EndLogEvent*>(history.events.back().get())) {
			history.events.push_back(shared_ptr<StateChange>(
				new EndLogEvent(getCurrentTime())));
		}

		// Transfer history to the new ReverseManager.
		// Also we should stop collecting in this ReverseManager,
		// and start collecting in the new one.
		ReverseManager& newManager = newBoard->getReverseManager();
		newManager.transferHistory(history, it->second.eventCount);

		// transfer (or copy) state from old to new machine
		transferState(*newBoard);

		// In case of load-replay it's possible we are not collecting,
		// but calling stop() anyway is ok.
		stop();

		// -- goto correct time within snapshot --
		// fast forward 2 frames before target time
		newBoard->fastForward(preTarget);

		// switch to the new MSXMotherBoard
		// TODO this is not correct if this board was not the active board
		reactor.replaceActiveBoard(newBoard);

		// Fast forward to actual target time with board activated.
		// This makes sure the video output gets rendered.
		newBoard->fastForward(targetTime);

		assert(!isCollecting());
		assert(newManager.isCollecting());
	} catch (MSXException&) {
		// Make sure mixer doesn't stay muted in case of error.
		mixer.unmute();
		throw;
	}
}

void ReverseManager::transferState(MSXMotherBoard& newBoard)
{
	// Transfer viewonly mode
	const StateChangeDistributor& oldDistributor =
		motherBoard.getStateChangeDistributor();
	StateChangeDistributor& newDistributor =
		newBoard.getStateChangeDistributor();
	newDistributor.setViewOnlyMode(oldDistributor.isViewOnlyMode());

	// transfer keyboard state
	ReverseManager& newManager = newBoard.getReverseManager();
	if (newManager.keyboard && keyboard) {
		newManager.keyboard->transferHostKeyMatrix(*keyboard);
	}

	// transfer watchpoints
	newBoard.getDebugger().transfer(motherBoard.getDebugger());

	// copy rerecord count
	newManager.reRecordCount = reRecordCount;
}

void ReverseManager::saveReplay(const vector<TclObject*>& tokens, TclObject& result)
{
	const Chunks& chunks = history.chunks;
	if (chunks.empty()) {
		throw CommandException("No recording...");
	}

	string filename;
	switch (tokens.size()) {
	case 2:
		// nothing
		break;
	case 3:
		filename = tokens[2]->getString();
		break;
	default:
		throw SyntaxError();
	}
	filename = FileOperations::parseCommandFileArgument(
		filename, REPLAY_DIR, "openmsx", ".omr");

	Reactor& reactor = motherBoard.getReactor();
	Replay replay(reactor);
	replay.reRecordCount = reRecordCount;

	// store current time (possibly somewhere in the middle of the timeline)
	// so that on load we can go back there
	replay.currentTime = motherBoard.getCurrentTime();

	// restore first snapshot to be able to serialize it to a file
	Reactor::Board initialBoard = reactor.createEmptyMotherBoard();
	MemInputArchive in(chunks.begin()->second.savestate->data(),
	                   chunks.begin()->second.savestate->size());
	in.serialize("machine", *initialBoard);
	replay.motherBoards.push_back(initialBoard);

	// determine which extra snapshots to put in the replay
	const EmuTime& startTime = chunks.begin()->second.time;
	const EmuTime& endTime   = chunks.rbegin()->second.time;
	EmuDuration totalLength = endTime - startTime;
	EmuDuration partitionLength = totalLength.divRoundUp(MAX_NOF_SNAPSHOTS);
	partitionLength = std::max(MIN_PARTITION_LENGTH, partitionLength);
	EmuTime nextPartitionEnd = startTime + partitionLength;
	Chunks::const_iterator it = chunks.begin();
	Chunks::const_iterator lastAddedIt = chunks.begin(); // already added
	while (it != chunks.end()) {
		++it;
		if (it == chunks.end() || (it->second.time > nextPartitionEnd)) {
			--it;
			assert(it->second.time <= nextPartitionEnd);
			if (it != lastAddedIt) {
				// this is a new one, add it to the list of snapshots
				Reactor::Board board = reactor.createEmptyMotherBoard();
				MemInputArchive in(it->second.savestate->data(),
				                   it->second.savestate->size());
				in.serialize("machine", *board);
				replay.motherBoards.push_back(board);
				lastAddedIt = it;
			}
			++it;
			while (it != chunks.end() && it->second.time > nextPartitionEnd) {
				nextPartitionEnd += partitionLength;
			}
		}
	}
	assert(lastAddedIt == --chunks.end()); // last snapshot must be included

	// add sentinel when there isn't one yet
	bool addSentinel = history.events.empty() ||
		!dynamic_cast<EndLogEvent*>(history.events.back().get());
	if (addSentinel) {
		/// make sure the replay log ends with a EndLogEvent
		history.events.push_back(shared_ptr<StateChange>(
			new EndLogEvent(getCurrentTime())));
	}
	try {
		XmlOutputArchive out(filename);
		replay.events = &history.events;
		out.serialize("replay", replay);
	} catch (MSXException&) {
		if (addSentinel) {
			history.events.pop_back();
		}
		throw;
	}

	if (addSentinel) {
		// Is there a cleaner way to only add the sentinel in the log?
		// I mean avoid changing/restoring the current log. We could
		// make a copy and work on that, but that seems much less
		// efficient.
		history.events.pop_back();
	}

	result.setString("Saved replay to " + filename);
}

void ReverseManager::loadReplay(const vector<TclObject*>& tokens, TclObject& result)
{
	if (tokens.size() < 3) throw SyntaxError();

	vector<string> arguments;
	TclObject* whereArg = NULL;
	bool enableViewOnly = false;

	for (unsigned i = 2; i < tokens.size(); ++i) {
		const string token = tokens[i]->getString();
		if (token == "-viewonly") {
			enableViewOnly = true;
		} else if (token == "-goto") {
			if (++i == tokens.size()) {
				throw CommandException("Missing argument");
			}
			whereArg = tokens[i];
		} else {
			arguments.push_back(token);
		}
	}

	if (arguments.size() != 1) throw SyntaxError();

	// resolve the filename
	UserDataFileContext context(REPLAY_DIR);
	string fileNameArg = arguments[0];
	string filename;
	try {
		// Try filename as typed by user.
		filename = context.resolve(fileNameArg);
	} catch (MSXException& /*e1*/) { try {
		// Not found, try adding '.omr'.
		filename = context.resolve(fileNameArg + ".omr");
	} catch (MSXException& e2) { try {
		// Again not found, try adding '.gz'.
		// (this is for backwards compatibility).
		filename = context.resolve(fileNameArg + ".gz");
	} catch (MSXException& /*e3*/) {
		// Show error message that includes the default extension.
		throw e2;
	}}}

	// restore replay
	Reactor& reactor = motherBoard.getReactor();
	Replay replay(reactor);
	Events events;
	replay.events = &events;
	try {
		XmlInputArchive in(filename);
		in.serialize("replay", replay);
	} catch (XMLException& e) {
		throw CommandException("Cannot load replay, bad file format: " + e.getMessage());
	} catch (MSXException& e) {
		throw CommandException("Cannot load replay: " + e.getMessage());
	}

	// get destination time index
	EmuTime destination = EmuTime::zero;
	string where = whereArg == NULL ? "begin" : whereArg->getString();
	if (where == "begin") {
		destination = EmuTime::zero;
	} else if (where == "end") {
		destination = EmuTime::infinity;
	} else if (where == "savetime") {
		destination = replay.currentTime;
	} else {
		destination += EmuDuration(whereArg->getDouble());
	}

	// OK, we are going to be actually changing states now

	// now we can change the view only mode
	motherBoard.getStateChangeDistributor().setViewOnlyMode(enableViewOnly);

	assert(!replay.motherBoards.empty());
	ReverseManager& newReverseManager = replay.motherBoards[0]->getReverseManager();
	ReverseHistory& newHistory = newReverseManager.history;

	if (newReverseManager.reRecordCount == 0) {
		// serialize Replay version >= 4
		newReverseManager.reRecordCount = replay.reRecordCount;
	} else {
		// newReverseManager.reRecordCount is initialized via
		// call from MSXMotherBoard to setReRecordCount()
	}

	// Restore event log
	swap(newHistory.events, events);
	Events& newEvents = newHistory.events;

	// Restore snapshots
	unsigned replayIndex = 0;
	for (MotherBoards::const_iterator it = replay.motherBoards.begin();
	     it != replay.motherBoards.end(); ++it) {
		ReverseChunk newChunk;
		newChunk.time = (*it)->getCurrentTime();

		MemOutputArchive out;
		out.serialize("machine", **it);
		newChunk.savestate = out.releaseBuffer();

		// update replayIndex
		// TODO: should we use <= instead??
		while (replayIndex < newEvents.size() &&
		       (newEvents[replayIndex]->getTime() < newChunk.time)) {
			replayIndex++;
		}
		newChunk.eventCount = replayIndex;

		newHistory.chunks[newHistory.getNextSeqNum(newChunk.time)] = newChunk;
	}

	// Note: untill this point we didn't make any changes to the current
	// ReverseManager/MSXMotherBoard yet
	reRecordCount = newReverseManager.reRecordCount;
	goTo(destination, newHistory);

	result.setString("Loaded replay from " + filename);
}

void ReverseManager::transferHistory(ReverseHistory& oldHistory,
                                     unsigned oldEventCount)
{
	assert(!isCollecting());
	assert(history.chunks.empty());

	// actual history transfer
	history.swap(oldHistory);

	// resume collecting (and event recording)
	collecting = true;
	schedule(getCurrentTime());
	motherBoard.getStateChangeDistributor().registerRecorder(*this);

	// start replaying events
	replayIndex = oldEventCount;
	// replay log contains at least the EndLogEvent
	assert(replayIndex < history.events.size());
	replayNextEvent();
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
			assert(!isReplaying()); // stopped by replay of EndLogEvent
		}
		break;
	}
}

int ReverseManager::signalEvent(shared_ptr<const Event> event)
{
	(void)event;
	assert(event->getType() == OPENMSX_TAKE_REVERSE_SNAPSHOT);

	// This event is send to all MSX machines, make sure it's actually this
	// machine that requested the snapshot.
	if (pendingTakeSnapshot) {
		pendingTakeSnapshot = false;
		takeSnapshot(getCurrentTime());
	}
	return 0;
}

unsigned ReverseManager::ReverseHistory::getNextSeqNum(EmuTime::param time) const
{
	if (chunks.empty()) {
		return 1;
	}
	const EmuTime& startTime = chunks.begin()->second.time;
	double duration = (time - startTime).toDouble();
	return unsigned(duration / SNAPSHOT_PERIOD + 0.5) + 1;
}

void ReverseManager::takeSnapshot(EmuTime::param time)
{
	// (possibly) drop old snapshots
	// TODO does snapshot pruning still happen correctly (often enough)
	//      when going back/forward in time?
	unsigned seqNum = history.getNextSeqNum(time);
	dropOldSnapshots<25>(seqNum);

	// During replay we might already have a snapshot with the current
	// sequence number, though this snapshot does not necessarily have the
	// exact same EmuTime (because we don't (re)start taking snapshots at
	// the same moment in time).

	// actually create new snapshot
	MemOutputArchive out;
	out.serialize("machine", motherBoard);
	ReverseChunk& newChunk = history.chunks[seqNum];
	newChunk.time = time;
	newChunk.savestate = out.releaseBuffer();
	newChunk.eventCount = replayIndex;

	// schedule creation of next snapshot
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
	if (isReplaying()) {
		// this is an event we just replayed
		assert(event == history.events[replayIndex]);
		if (dynamic_cast<EndLogEvent*>(event.get())) {
			motherBoard.getStateChangeDistributor().stopReplay(
				event->getTime());
			// this is needed to prevent a reRecordCount increase
			// due to this sentinel ending the replay
			reRecordCount--;
		} else {
			// ignore all other events
		}
	} else {
		// record event
		history.events.push_back(event);
		++replayIndex;
		assert(!isReplaying());
	}
}

void ReverseManager::stopReplay(EmuTime::param time)
{
	if (isReplaying()) {
		// if we're replaying, stop it and erase remainder of event log
		removeSyncPoint(INPUT_EVENT);
		Events& events = history.events;
		events.erase(events.begin() + replayIndex, events.end());
		// search snapshots that are newer than 'time' and erase them
		Chunks::iterator it = history.chunks.begin();
		while ((it != history.chunks.end()) &&
		       (it->second.time <= time)) {
			++it;
		}
		history.chunks.erase(it, history.chunks.end());
		// this also means someone is changing history, record that
		reRecordCount++;
	}
	assert(!isReplaying());
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
	setSyncPoint(time + EmuDuration(SNAPSHOT_PERIOD), NEW_SNAPSHOT);
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
	} else if (subcommand == "status") {
		manager.status(result);
	} else if (subcommand == "debug") {
		manager.debugInfo(result);
	} else if (subcommand == "goback") {
		manager.goBack(tokens);
	} else if (subcommand == "goto") {
		manager.goTo(tokens);
	} else if (subcommand == "savereplay") {
		return manager.saveReplay(tokens, result);
	} else if (subcommand == "loadreplay") {
		return manager.loadReplay(tokens, result);
	} else if (subcommand == "viewonlymode") {
		StateChangeDistributor& distributor = manager.motherBoard.getStateChangeDistributor();
		switch (tokens.size()) {
		case 2:
			result.setString(distributor.isViewOnlyMode() ? "true" : "false");
			break;
		case 3:
			distributor.setViewOnlyMode(tokens[2]->getBoolean());
			break;
		default:
			throw SyntaxError();
		}
	} else {
		throw CommandException("Invalid subcommand: " + subcommand);
	}
}

string ReverseCmd::help(const vector<string>& /*tokens*/) const
{
	return "start               start collecting reverse data\n"
	       "stop                stop collecting\n"
	       "status              show various status info on reverse\n"
	       "goback <n>          go back <n> seconds in time\n"
	       "goto <time>         go to an absolute moment in time\n"
	       "viewonlymode <bool> switch viewonly mode on or off\n"
	       "savereplay [<name>] save the first snapshot and all replay data as a 'replay' (with optional name)\n"
	       "loadreplay [-goto <begin|end|savetime|<n>>] [-viewonly] <name>   load a replay (snapshot and replay data) with given name and start replaying\n";
}

void ReverseCmd::tabCompletion(vector<string>& tokens) const
{
	if (tokens.size() == 2) {
		set<string> subCommands;
		subCommands.insert("start");
		subCommands.insert("stop");
		subCommands.insert("status");
		subCommands.insert("goback");
		subCommands.insert("goto");
		subCommands.insert("savereplay");
		subCommands.insert("loadreplay");
		subCommands.insert("viewonlymode");
		completeString(tokens, subCommands);
	} else if ((tokens.size() == 3) || (tokens[1] == "loadreplay")) {
		if (tokens[1] == "loadreplay" || tokens[1] == "savereplay") {
			std::set<string> cmds;
			if (tokens[1] == "loadreplay") {
				const char* const str[2] = { "-goto", "-viewonly" };
				cmds = std::set<string>(str, str + 2);
			}
			UserDataFileContext context(REPLAY_DIR);
			completeFileName(tokens, context, cmds);
		} else if (tokens[1] == "viewonlymode") {
			set<string> options;
			options.insert("true");
			options.insert("false");
			completeString(tokens, options);
		}
	}
}

} // namespace openmsx
