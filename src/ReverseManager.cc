#include "ReverseManager.hh"
#include "MSXMotherBoard.hh"
#include "EventDistributor.hh"
#include "StateChangeDistributor.hh"
#include "Keyboard.hh"
#include "Debugger.hh"
#include "EventDelay.hh"
#include "MSXMixer.hh"
#include "MSXCommandController.hh"
#include "XMLException.hh"
#include "TclObject.hh"
#include "FileOperations.hh"
#include "FileContext.hh"
#include "StateChange.hh"
#include "Reactor.hh"
#include "CommandException.hh"
#include "MemBuffer.hh"
#include "StringOp.hh"
#include "serialize.hh"
#include "serialize_stl.hh"
#include "xrange.hh"
#include <functional>
#include <cassert>

using std::string;
using std::vector;
using std::shared_ptr;
using std::move;

namespace openmsx {

// Time between two snapshots (in seconds)
static const double SNAPSHOT_PERIOD = 1.0;

// Max number of snapshots in a replay file
static const unsigned MAX_NOF_SNAPSHOTS = 10;

// Min distance between snapshots in replay file (in seconds)
static const EmuDuration MIN_PARTITION_LENGTH = EmuDuration(60.0);

// Max distance of one before last snapshot before the end time in replay file (in seconds)
static const EmuDuration MAX_DIST_1_BEFORE_LAST_SNAPSHOT = EmuDuration(30.0);

static const char* const REPLAY_DIR = "replays";

// A replay is a struct that contains a vector of motherboards and an MSX event
// log. Those combined are a replay, because you can replay the events from an
// existing motherboard state: the vector has to have at least one motherboard
// (the initial state), but can have optionally more motherboards, which are
// merely in-between snapshots, so it is quicker to jump to a later time in the
// event log.

struct Replay
{
	Replay(Reactor& reactor_)
		: reactor(reactor_), currentTime(EmuTime::dummy()) {}

	Reactor& reactor;

	ReverseManager::Events* events;
	std::vector<Reactor::Board> motherBoards;
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
			ar.serializeWithID("snapshots", motherBoards, std::ref(reactor));
		} else {
			Reactor::Board newBoard = reactor.createEmptyMotherBoard();
			ar.serialize("snapshot", *newBoard);
			motherBoards.push_back(move(newBoard));
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


class EndLogEvent final : public StateChange
{
public:
	EndLogEvent() {} // for serialize
	EndLogEvent(EmuTime::param time_)
		: StateChange(time_)
	{
	}

	template<typename Archive> void serialize(Archive& ar, unsigned /*version*/)
	{
		ar.template serializeBase<StateChange>(*this);
	}
};
REGISTER_POLYMORPHIC_CLASS(StateChange, EndLogEvent, "EndLog");

// class ReverseManager

ReverseManager::ReverseManager(MSXMotherBoard& motherBoard_)
	: syncNewSnapshot(motherBoard_.getScheduler())
	, syncInputEvent (motherBoard_.getScheduler())
	, motherBoard(motherBoard_)
	, eventDistributor(motherBoard.getReactor().getEventDistributor())
	, reverseCmd(motherBoard.getCommandController())
	, keyboard(nullptr)
	, eventDelay(nullptr)
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
		// schedule creation of next snapshot
		schedule(getCurrentTime());
		// start recording events
		motherBoard.getStateChangeDistributor().registerRecorder(*this);
	}
	assert(isCollecting());
}

void ReverseManager::stop()
{
	if (isCollecting()) {
		motherBoard.getStateChangeDistributor().unregisterRecorder(*this);
		syncNewSnapshot.removeSyncPoint(); // don't schedule new snapshot takings
		syncInputEvent .removeSyncPoint(); // stop any pending replay actions
		history.clear();
		replayIndex = 0;
		collecting = false;
		pendingTakeSnapshot = false;
	}
	assert(!pendingTakeSnapshot);
	assert(!isCollecting());
	assert(!isReplaying());
}

EmuTime::param ReverseManager::getEndTime(const ReverseHistory& hist) const
{
	if (!hist.events.empty()) {
		if (auto* ev = dynamic_cast<const EndLogEvent*>(
				hist.events.back().get())) {
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
	EmuTime b(isCollecting() ? begin(history.chunks)->second.time
	                         : EmuTime::zero);
	result.addListElement((b - EmuTime::zero).toDouble());

	result.addListElement("end");
	EmuTime end(isCollecting() ? getEndTime(history) : EmuTime::zero);
	result.addListElement((end - EmuTime::zero).toDouble());

	result.addListElement("current");
	EmuTime current(isCollecting() ? getCurrentTime() : EmuTime::zero);
	result.addListElement((current - EmuTime::zero).toDouble());

	result.addListElement("snapshots");
	TclObject snapshots;
	for (auto& p : history.chunks) {
		EmuTime time = p.second.time;
		snapshots.addListElement((time - EmuTime::zero).toDouble());
	}
	result.addListElement(snapshots);

	result.addListElement("last_event");
	auto lastEvent = history.events.rbegin();
	if (lastEvent != history.events.rend() && dynamic_cast<const EndLogEvent*>(lastEvent->get())) {
		++lastEvent;
	}
	EmuTime le(isCollecting() && (lastEvent != history.events.rend()) ? (*lastEvent)->getTime() : EmuTime::zero);
	result.addListElement((le - EmuTime::zero).toDouble());
}

void ReverseManager::debugInfo(TclObject& result) const
{
	// TODO this is useful during development, but for the end user this
	// information means nothing. We should remove this later.
	StringOp::Builder res;
	size_t totalSize = 0;
	for (auto& p : history.chunks) {
		auto& chunk = p.second;
		res << p.first << ' '
		    << (chunk.time - EmuTime::zero).toDouble() << ' '
		    << ((chunk.time - EmuTime::zero).toDouble() / (getCurrentTime() - EmuTime::zero).toDouble()) * 100 << '%'
		    << " (" << chunk.size << ')'
		    << " (next event index: " << chunk.eventCount << ")\n";
		totalSize += chunk.size;
	}
	res << "total size: " << totalSize << '\n';
	result.setString(string(res));
}

static void parseGoTo(Interpreter& interp, array_ref<TclObject> tokens,
                      bool& novideo, double& time)
{
	novideo = false;
	bool hasTime = false;
	for (auto i : xrange(size_t(2), tokens.size())) {
		if (tokens[i] == "-novideo") {
			novideo = true;
		} else {
			time = tokens[i].getDouble(interp);
			hasTime = true;
		}
	}
	if (!hasTime) {
		throw SyntaxError();
	}
}

void ReverseManager::goBack(array_ref<TclObject> tokens)
{
	bool novideo;
	double t;
	auto& interp = motherBoard.getReactor().getInterpreter();
	parseGoTo(interp, tokens, novideo, t);

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
	goTo(target, novideo);
}

void ReverseManager::goTo(array_ref<TclObject> tokens)
{
	bool novideo;
	double t;
	auto& interp = motherBoard.getReactor().getInterpreter();
	parseGoTo(interp, tokens, novideo, t);

	EmuTime target = EmuTime::zero + EmuDuration(t);
	goTo(target, novideo);
}

void ReverseManager::goTo(EmuTime::param target, bool novideo)
{
	if (!isCollecting()) {
		throw CommandException(
			"Reverse was not enabled. First execute the 'reverse "
			"start' command to start collecting data.");
	}
	goTo(target, novideo, history, true); // move in current time-line
}

void ReverseManager::goTo(
	EmuTime::param target, bool novideo, ReverseHistory& hist,
	bool sameTimeLine)
{
	auto& mixer = motherBoard.getMSXMixer();
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
		assert(!hist.chunks.empty());
		auto it = begin(hist.chunks);
		EmuTime firstTime = it->second.time;
		EmuTime targetTime = std::max(target, firstTime);
		// Also don't go further into the future than 'end time'.
		targetTime = std::min(targetTime, getEndTime(hist));

		// Duration of 2 PAL frames. Possible improvement is to use the
		// actual refresh rate (PAL/NTSC). But it should be the refresh
		// rate of the active video chip (v99x8/v9990) at the target
		// time. This is quite complex to get and the difference between
		// 2 PAL and 2 NTSC frames isn't that big.
		double dur2frames = 2.0 * (313.0 * 1368.0) / (3579545.0 * 6.0);
		EmuDuration preDelta(novideo ? 0.0 : dur2frames);
		EmuTime preTarget = ((targetTime - firstTime) > preDelta)
		                  ? targetTime - preDelta
		                  : firstTime;

		// find oldest snapshot that is not newer than requested time
		// TODO ATM we do a linear search, could be improved to do a binary search.
		assert(it->second.time <= preTarget); // first one is not newer
		assert(it != end(hist.chunks)); // there are snapshots
		do {
			++it;
		} while (it != end(hist.chunks) &&
			 it->second.time <= preTarget);
		// We found the first one that's newer, previous one is last
		// one that's not newer (thus older or equal).
		assert(it != begin(hist.chunks));
		--it;
		EmuTime snapshotTime = it->second.time;
		assert(snapshotTime <= preTarget);

		// IF current time is before the wanted time AND either
		//   - current time is closer than the closest (earlier) snapshot
		//   - OR current time is close enough (I arbitrarily choose 1s)
		// THEN it's cheaper to start from the current position (and
		//      emulated forward) than to start from a snapshot
		// THOUGH only when we're currently in the same time-line
		//   e.g. OK for a 'reverse goto' command, but not for a
		//   'reverse loadreplay' command.
		auto& reactor = motherBoard.getReactor();
		EmuTime currentTime = getCurrentTime();
		MSXMotherBoard* newBoard;
		Reactor::Board newBoard_; // either nullptr or the same as newBoard
		if (sameTimeLine &&
		    (currentTime <= preTarget) &&
		    ((snapshotTime <= currentTime) ||
		     ((preTarget - currentTime) < EmuDuration(1.0)))) {
			newBoard = &motherBoard; // use current board
		} else {
			// Note: we don't (anymore) erase future snapshots
			// -- restore old snapshot --
			newBoard_ = reactor.createEmptyMotherBoard();
			newBoard = newBoard_.get();
			MemInputArchive in(it->second.savestate.data(),
					   it->second.size);
			in.serialize("machine", *newBoard);

			if (eventDelay) {
				// Handle all events that are scheduled, but not yet
				// distributed. This makes sure no events get lost
				// (important to keep host/msx keyboard in sync).
				eventDelay->flush();
			}

			// terminate replay log with EndLogEvent (if not there already)
			if (hist.events.empty() ||
			    !dynamic_cast<const EndLogEvent*>(hist.events.back().get())) {
				hist.events.push_back(
					std::make_shared<EndLogEvent>(currentTime));
			}

			// Transfer history to the new ReverseManager.
			// Also we should stop collecting in this ReverseManager,
			// and start collecting in the new one.
			auto& newManager = newBoard->getReverseManager();
			newManager.transferHistory(hist, it->second.eventCount);

			// transfer (or copy) state from old to new machine
			transferState(*newBoard);

			// In case of load-replay it's possible we are not collecting,
			// but calling stop() anyway is ok.
			stop();
		}

		// -- goto correct time within snapshot --
		// Fast forward 2 frames before target time.
		// If we're short on snapshots, create them at intervals that are
		// at least the usual interval, but the later, the more: each
		// time divide the remaining time in half and make a snapshot
		// there.
		while (true) {
			EmuTime nextTarget = std::min(
				preTarget,
				newBoard->getCurrentTime() + std::max(
					EmuDuration(SNAPSHOT_PERIOD),
					(preTarget - newBoard->getCurrentTime()) / 2
					));
			newBoard->fastForward(nextTarget, true);
			// note: fastForward does not always stop at
			//       _exactly_ the requested time
			if (newBoard->getCurrentTime() >= preTarget) break;
			newBoard->getReverseManager().takeSnapshot(
				newBoard->getCurrentTime());
		}

		// switch to the new MSXMotherBoard
		//  Note: this deletes the current MSXMotherBoard and
		//  ReverseManager. So we can't access those objects anymore.
		bool unmute = true;
		if (newBoard_) {
			unmute = false;
			reactor.replaceBoard(motherBoard, move(newBoard_));
		}

		// Fast forward to actual target time with board activated.
		// This makes sure the video output gets rendered.
		newBoard->fastForward(targetTime, false);

		// In case we didn't actually create a new board, don't leave
		// the (old) board muted.
		if (unmute) {
			mixer.unmute();
		}

		//assert(!isCollecting()); // can't access 'this->' members anymore!
		assert(newBoard->getReverseManager().isCollecting());
	} catch (MSXException&) {
		// Make sure mixer doesn't stay muted in case of error.
		mixer.unmute();
		throw;
	}
}

void ReverseManager::transferState(MSXMotherBoard& newBoard)
{
	// Transfer viewonly mode
	const auto& oldDistributor = motherBoard.getStateChangeDistributor();
	      auto& newDistributor = newBoard   .getStateChangeDistributor();
	newDistributor.setViewOnlyMode(oldDistributor.isViewOnlyMode());

	// transfer keyboard state
	auto& newManager = newBoard.getReverseManager();
	if (newManager.keyboard && keyboard) {
		newManager.keyboard->transferHostKeyMatrix(*keyboard);
	}

	// transfer watchpoints
	newBoard.getDebugger().transfer(motherBoard.getDebugger());

	// copy rerecord count
	newManager.reRecordCount = reRecordCount;

	// transfer settings
	const auto& oldController = motherBoard.getMSXCommandController();
	newBoard.getMSXCommandController().transferSettings(oldController);
}

void ReverseManager::saveReplay(
	Interpreter& interp, array_ref<TclObject> tokens, TclObject& result)
{
	const auto& chunks = history.chunks;
	if (chunks.empty()) {
		throw CommandException("No recording...");
	}

	string filename;
	int maxNofExtraSnapshots = MAX_NOF_SNAPSHOTS;
	switch (tokens.size()) {
	case 2:
		// nothing
		break;
	case 3:
		filename = tokens[2].getString().str();
		break;
	case 4:
	case 5:
		size_t tn;
		for (tn = 2; tn < (tokens.size() - 1); ++tn) {
			if (tokens[tn] == "-maxnofextrasnapshots") {
				maxNofExtraSnapshots = tokens[tn + 1].getInt(interp);
				break;
			}
		}
		if (tn == (tokens.size() - 1)) throw SyntaxError();
		if (tokens.size() == 5) {
			filename = tokens[tn == 2 ? 4 : 2].getString().str();
		}
		if (maxNofExtraSnapshots < 0) {
			throw CommandException("Maximum number of snapshots should be at least 0");
		}

		break;
	default:
		throw SyntaxError();
	}
	filename = FileOperations::parseCommandFileArgument(
		filename, REPLAY_DIR, "openmsx", ".omr");

	auto& reactor = motherBoard.getReactor();
	Replay replay(reactor);
	replay.reRecordCount = reRecordCount;

	// store current time (possibly somewhere in the middle of the timeline)
	// so that on load we can go back there
	replay.currentTime = getCurrentTime();

	// restore first snapshot to be able to serialize it to a file
	auto initialBoard = reactor.createEmptyMotherBoard();
	MemInputArchive in(begin(chunks)->second.savestate.data(),
	                   begin(chunks)->second.size);
	in.serialize("machine", *initialBoard);
	replay.motherBoards.push_back(move(initialBoard));

	if (maxNofExtraSnapshots > 0) {
		// determine which extra snapshots to put in the replay
		const auto& startTime = begin(chunks)->second.time;
		// for the end time, try to take MAX_DIST_1_BEFORE_LAST_SNAPSHOT
		// seconds before the normal end time so that we get an extra snapshot
		// at that point, which is comfortable if you want to reverse from the
		// last snapshot after loading the replay.
		const auto& lastChunkTime = chunks.rbegin()->second.time;
		const auto& endTime   = ((startTime + MAX_DIST_1_BEFORE_LAST_SNAPSHOT) < lastChunkTime) ? lastChunkTime - MAX_DIST_1_BEFORE_LAST_SNAPSHOT : lastChunkTime;
		EmuDuration totalLength = endTime - startTime;
		EmuDuration partitionLength = totalLength.divRoundUp(maxNofExtraSnapshots);
		partitionLength = std::max(MIN_PARTITION_LENGTH, partitionLength);
		EmuTime nextPartitionEnd = startTime + partitionLength;
		auto it = begin(chunks);
		auto lastAddedIt = begin(chunks); // already added
		while (it != end(chunks)) {
			++it;
			if (it == end(chunks) || (it->second.time > nextPartitionEnd)) {
				--it;
				assert(it->second.time <= nextPartitionEnd);
				if (it != lastAddedIt) {
					// this is a new one, add it to the list of snapshots
					Reactor::Board board = reactor.createEmptyMotherBoard();
					MemInputArchive in2(it->second.savestate.data(),
							    it->second.size);
					in2.serialize("machine", *board);
					replay.motherBoards.push_back(move(board));
					lastAddedIt = it;
				}
				++it;
				while (it != end(chunks) && it->second.time > nextPartitionEnd) {
					nextPartitionEnd += partitionLength;
				}
			}
		}
		assert(lastAddedIt == --end(chunks)); // last snapshot must be included
	}

	// add sentinel when there isn't one yet
	bool addSentinel = history.events.empty() ||
		!dynamic_cast<EndLogEvent*>(history.events.back().get());
	if (addSentinel) {
		/// make sure the replay log ends with a EndLogEvent
		history.events.push_back(std::make_shared<EndLogEvent>(
			getCurrentTime()));
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

void ReverseManager::loadReplay(
	Interpreter& interp, array_ref<TclObject> tokens, TclObject& result)
{
	if (tokens.size() < 3) throw SyntaxError();

	vector<string> arguments;
	const TclObject* whereArg = nullptr;
	bool enableViewOnly = false;

	for (size_t i = 2; i < tokens.size(); ++i) {
		string_ref token = tokens[i].getString();
		if (token == "-viewonly") {
			enableViewOnly = true;
		} else if (token == "-goto") {
			if (++i == tokens.size()) {
				throw CommandException("Missing argument");
			}
			whereArg = &tokens[i];
		} else {
			arguments.push_back(token.str());
		}
	}

	if (arguments.size() != 1) throw SyntaxError();

	// resolve the filename
	auto context = userDataFileContext(REPLAY_DIR);
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
	auto& reactor = motherBoard.getReactor();
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
	auto destination = EmuTime::zero;
	string_ref where = whereArg ? whereArg->getString() : "begin";
	if (where == "begin") {
		destination = EmuTime::zero;
	} else if (where == "end") {
		destination = EmuTime::infinity;
	} else if (where == "savetime") {
		destination = replay.currentTime;
	} else {
		destination += EmuDuration(whereArg->getDouble(interp));
	}

	// OK, we are going to be actually changing states now

	// now we can change the view only mode
	motherBoard.getStateChangeDistributor().setViewOnlyMode(enableViewOnly);

	assert(!replay.motherBoards.empty());
	auto& newReverseManager = replay.motherBoards[0]->getReverseManager();
	auto& newHistory = newReverseManager.history;

	if (newReverseManager.reRecordCount == 0) {
		// serialize Replay version >= 4
		newReverseManager.reRecordCount = replay.reRecordCount;
	} else {
		// newReverseManager.reRecordCount is initialized via
		// call from MSXMotherBoard to setReRecordCount()
	}

	// Restore event log
	swap(newHistory.events, events);
	auto& newEvents = newHistory.events;

	// Restore snapshots
	unsigned replayIdx = 0;
	for (auto& m : replay.motherBoards) {
		ReverseChunk newChunk;
		newChunk.time = m->getCurrentTime();

		MemOutputArchive out;
		out.serialize("machine", *m);
		newChunk.savestate = out.releaseBuffer(newChunk.size);

		// update replayIdx
		// TODO: should we use <= instead??
		while (replayIdx < newEvents.size() &&
		       (newEvents[replayIdx]->getTime() < newChunk.time)) {
			replayIdx++;
		}
		newChunk.eventCount = replayIdx;

		newHistory.chunks[newHistory.getNextSeqNum(newChunk.time)] =
			move(newChunk);
	}

	// Note: untill this point we didn't make any changes to the current
	// ReverseManager/MSXMotherBoard yet
	reRecordCount = newReverseManager.reRecordCount;
	bool novideo = false;
	goTo(destination, novideo, newHistory, false); // move to different time-line

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

void ReverseManager::execNewSnapshot()
{
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
		std::make_shared<SimpleEvent>(OPENMSX_TAKE_REVERSE_SNAPSHOT));
}

void ReverseManager::execInputEvent()
{
	auto event = history.events[replayIndex];
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
}

int ReverseManager::signalEvent(const shared_ptr<const Event>& event)
{
	(void)event;
	assert(event->getType() == OPENMSX_TAKE_REVERSE_SNAPSHOT);

	// This event is send to all MSX machines, make sure it's actually this
	// machine that requested the snapshot.
	if (pendingTakeSnapshot) {
		pendingTakeSnapshot = false;
		takeSnapshot(getCurrentTime());
		// schedule creation of next snapshot
		schedule(getCurrentTime());
	}
	return 0;
}

unsigned ReverseManager::ReverseHistory::getNextSeqNum(EmuTime::param time) const
{
	if (chunks.empty()) {
		return 0;
	}
	const auto& startTime = begin(chunks)->second.time;
	double duration = (time - startTime).toDouble();
	return unsigned(duration / SNAPSHOT_PERIOD + 0.5);
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
	newChunk.savestate = out.releaseBuffer(newChunk.size);
	newChunk.eventCount = replayIndex;
}

void ReverseManager::replayNextEvent()
{
	// schedule next event at its own time
	assert(replayIndex < history.events.size());
	syncInputEvent.setSyncPoint(history.events[replayIndex]->getTime());
}

void ReverseManager::signalStateChange(const shared_ptr<StateChange>& event)
{
	if (isReplaying()) {
		// this is an event we just replayed
		assert(event == history.events[replayIndex]);
		if (dynamic_cast<EndLogEvent*>(event.get())) {
			signalStopReplay(event->getTime());
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

void ReverseManager::signalStopReplay(EmuTime::param time)
{
	motherBoard.getStateChangeDistributor().stopReplay(time);
	// this is needed to prevent a reRecordCount increase
	// due to this action ending the replay
	reRecordCount--;
}

void ReverseManager::stopReplay(EmuTime::param time)
{
	if (isReplaying()) {
		// if we're replaying, stop it and erase remainder of event log
		syncInputEvent.removeSyncPoint();
		Events& events = history.events;
		events.erase(begin(events) + replayIndex, end(events));
		// search snapshots that are newer than 'time' and erase them
		auto it = find_if(begin(history.chunks), end(history.chunks),
			[&](Chunks::value_type& p) { return p.second.time > time; });
		history.chunks.erase(it, end(history.chunks));
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
	unsigned y = (count + N) ^ (count + N + 1);
	unsigned d = N;
	unsigned d2 = 2 * N + 1;
	while (true) {
		y >>= 1;
		if ((y == 0) || (count < d)) return;
		history.chunks.erase(count - d);
		d += d2;
		d2 *= 2;
	}
}

void ReverseManager::schedule(EmuTime::param time)
{
	syncNewSnapshot.setSyncPoint(time + EmuDuration(SNAPSHOT_PERIOD));
}


// class ReverseCmd

ReverseManager::ReverseCmd::ReverseCmd(CommandController& controller)
	: Command(controller, "reverse")
{
}

void ReverseManager::ReverseCmd::execute(array_ref<TclObject> tokens, TclObject& result)
{
	if (tokens.size() < 2) {
		throw CommandException("Missing subcommand");
	}
	auto& manager = OUTER(ReverseManager, reverseCmd);
	auto& interp = getInterpreter();
	string_ref subcommand = tokens[1].getString();
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
		return manager.saveReplay(interp, tokens, result);
	} else if (subcommand == "loadreplay") {
		return manager.loadReplay(interp, tokens, result);
	} else if (subcommand == "viewonlymode") {
		auto& distributor = manager.motherBoard.getStateChangeDistributor();
		switch (tokens.size()) {
		case 2:
			result.setString(distributor.isViewOnlyMode() ? "true" : "false");
			break;
		case 3:
			distributor.setViewOnlyMode(tokens[2].getBoolean(interp));
			break;
		default:
			throw SyntaxError();
		}
	} else if (subcommand == "truncatereplay") {
		if (manager.isReplaying()) {
			manager.signalStopReplay(manager.getCurrentTime());
		}
	} else {
		throw CommandException("Invalid subcommand: " + subcommand);
	}
}

string ReverseManager::ReverseCmd::help(const vector<string>& /*tokens*/) const
{
	return "start               start collecting reverse data\n"
	       "stop                stop collecting\n"
	       "status              show various status info on reverse\n"
	       "goback <n>          go back <n> seconds in time\n"
	       "goto <time>         go to an absolute moment in time\n"
	       "viewonlymode <bool> switch viewonly mode on or off\n"
	       "truncatereplay      stop replaying and remove all 'future' data\n"
	       "savereplay [<name>] save the first snapshot and all replay data as a 'replay' (with optional name)\n"
	       "loadreplay [-goto <begin|end|savetime|<n>>] [-viewonly] <name>   load a replay (snapshot and replay data) with given name and start replaying\n";
}

void ReverseManager::ReverseCmd::tabCompletion(vector<string>& tokens) const
{
	if (tokens.size() == 2) {
		static const char* const subCommands[] = {
			"start", "stop", "status", "goback", "goto",
			"savereplay", "loadreplay", "viewonlymode",
			"truncatereplay",
		};
		completeString(tokens, subCommands);
	} else if ((tokens.size() == 3) || (tokens[1] == "loadreplay")) {
		if (tokens[1] == "loadreplay" || tokens[1] == "savereplay") {
			std::vector<const char*> cmds;
			if (tokens[1] == "loadreplay") {
				cmds = { "-goto", "-viewonly" };
			}
			completeFileName(tokens, userDataFileContext(REPLAY_DIR), cmds);
		} else if (tokens[1] == "viewonlymode") {
			static const char* const options[] = { "true", "false" };
			completeString(tokens, options);
		}
	}
}

} // namespace openmsx
