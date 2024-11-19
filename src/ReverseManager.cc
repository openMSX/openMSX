#include "ReverseManager.hh"

#include "CommandException.hh"
#include "Debugger.hh"
#include "Display.hh"
#include "Event.hh"
#include "EventDelay.hh"
#include "EventDistributor.hh"
#include "FileContext.hh"
#include "FileOperations.hh"
#include "Keyboard.hh"
#include "MSXCliComm.hh"
#include "MSXCommandController.hh"
#include "MSXMixer.hh"
#include "MSXMotherBoard.hh"
#include "Reactor.hh"
#include "StateChange.hh"
#include "StateChangeDistributor.hh"
#include "TclArgParser.hh"
#include "TclObject.hh"
#include "Timer.hh"
#include "XMLException.hh"
#include "serialize.hh"
#include "serialize_meta.hh"

#include "narrow.hh"
#include "one_of.hh"
#include "ranges.hh"
#include "view.hh"

#include <array>
#include <cassert>
#include <cmath>
#include <iomanip>

namespace openmsx {

// Time between two snapshots (in seconds)
static constexpr double SNAPSHOT_PERIOD = 1.0;

// Max number of snapshots in a replay file
static constexpr unsigned MAX_NOF_SNAPSHOTS = 10;

// Min distance between snapshots in replay file (in seconds)
static constexpr auto MIN_PARTITION_LENGTH = EmuDuration(60.0);

// Max distance of one before last snapshot before the end time in replay file (in seconds)
static constexpr auto MAX_DIST_1_BEFORE_LAST_SNAPSHOT = EmuDuration(30.0);

// A replay is a struct that contains a vector of motherboards and an MSX event
// log. Those combined are a replay, because you can replay the events from an
// existing motherboard state: the vector has to have at least one motherboard
// (the initial state), but can have optionally more motherboards, which are
// merely in-between snapshots, so it is quicker to jump to a later time in the
// event log.

struct Replay
{
	explicit Replay(Reactor& reactor_)
		: reactor(reactor_) {}

	Reactor& reactor;

	ReverseManager::Events* events;
	std::vector<Reactor::Board> motherBoards;
	EmuTime currentTime = EmuTime::dummy();
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
			motherBoards.push_back(std::move(newBoard));
		}

		ar.serialize("events", *events);

		if (ar.versionAtLeast(version, 3)) {
			ar.serialize("currentTime", currentTime);
		} else {
			assert(Archive::IS_LOADER);
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

void ReverseManager::ReverseHistory::swap(ReverseHistory& other) noexcept
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
	EndLogEvent() = default; // for serialize
	explicit EndLogEvent(EmuTime::param time_)
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
{
	eventDistributor.registerEventListener(EventType::TAKE_REVERSE_SNAPSHOT, *this);

	assert(!isCollecting());
	assert(!isReplaying());
}

ReverseManager::~ReverseManager()
{
	stop();
	eventDistributor.unregisterEventListener(EventType::TAKE_REVERSE_SNAPSHOT, *this);
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
		if (const auto* ev = dynamic_cast<const EndLogEvent*>(
				hist.events.back().get())) {
			// last log element is EndLogEvent, use that
			return ev->getTime();
		}
	}
	// otherwise use current time
	assert(!isReplaying());
	return getCurrentTime();
}

bool ReverseManager::isViewOnlyMode() const
{
	return motherBoard.getStateChangeDistributor().isViewOnlyMode();
}
double ReverseManager::getBegin() const
{
	EmuTime b(isCollecting() ? begin(history.chunks)->second.time
	                         : EmuTime::zero());
	return (b - EmuTime::zero()).toDouble();
}
double ReverseManager::getEnd() const
{
	EmuTime end(isCollecting() ? getEndTime(history) : EmuTime::zero());
	return (end - EmuTime::zero()).toDouble();
}
double ReverseManager::getCurrent() const
{
	EmuTime current(isCollecting() ? getCurrentTime() : EmuTime::zero());
	return (current - EmuTime::zero()).toDouble();
}

void ReverseManager::status(TclObject& result) const
{
	result.addDictKeyValue("status", !isCollecting() ? "disabled"
	                               : isReplaying()   ? "replaying"
	                                                 : "enabled");
	result.addDictKeyValue("begin", getBegin());
	result.addDictKeyValue("end", getEnd());
	result.addDictKeyValue("current", getCurrent());

	TclObject snapshots;
	snapshots.addListElements(view::transform(history.chunks, [](auto& p) {
		return (p.second.time - EmuTime::zero()).toDouble();
	}));
	result.addDictKeyValue("snapshots", snapshots);

	auto lastEvent = rbegin(history.events);
	if (lastEvent != rend(history.events) && dynamic_cast<const EndLogEvent*>(lastEvent->get())) {
		++lastEvent;
	}
	EmuTime le(isCollecting() && (lastEvent != rend(history.events)) ? (*lastEvent)->getTime() : EmuTime::zero());
	result.addDictKeyValue("last_event", (le - EmuTime::zero()).toDouble());
}

void ReverseManager::debugInfo(TclObject& result) const
{
	// TODO this is useful during development, but for the end user this
	// information means nothing. We should remove this later.
	std::string res;
	size_t totalSize = 0;
	for (const auto& [idx, chunk] : history.chunks) {
		strAppend(res, idx, ' ',
		          (chunk.time - EmuTime::zero()).toDouble(), ' ',
		          ((chunk.time - EmuTime::zero()).toDouble() / (getCurrentTime() - EmuTime::zero()).toDouble()) * 100, "%"
		          " (", chunk.savestate.size(), ")"
		          " (next event index: ", chunk.eventCount, ")\n");
		totalSize += chunk.savestate.size();
	}
	strAppend(res, "total size: ", totalSize, '\n');
	result = res;
}

static std::pair<bool, double> parseGoTo(Interpreter& interp, std::span<const TclObject> tokens)
{
	bool noVideo = false;
	std::array info = {flagArg("-novideo", noVideo)};
	auto args = parseTclArgs(interp, tokens.subspan(2), info);
	if (args.size() != 1) throw SyntaxError();
	double time = args[0].getDouble(interp);
	return {noVideo, time};
}

void ReverseManager::goBack(std::span<const TclObject> tokens)
{
	auto& interp = motherBoard.getReactor().getInterpreter();
	auto [noVideo, t] = parseGoTo(interp, tokens);

	EmuTime now = getCurrentTime();
	EmuTime target(EmuTime::dummy());
	if (t >= 0) {
		EmuDuration d(t);
		if (d < (now - EmuTime::zero())) {
			target = now - d;
		} else {
			target = EmuTime::zero();
		}
	} else {
		target = now + EmuDuration(-t);
	}
	goTo(target, noVideo);
}

void ReverseManager::goTo(std::span<const TclObject> tokens)
{
	auto& interp = motherBoard.getReactor().getInterpreter();
	auto [noVideo, t] = parseGoTo(interp, tokens);

	EmuTime target = EmuTime::zero() + EmuDuration(t);
	goTo(target, noVideo);
}

void ReverseManager::goTo(EmuTime::param target, bool noVideo)
{
	if (!isCollecting()) {
		throw CommandException(
			"Reverse was not enabled. First execute the 'reverse "
			"start' command to start collecting data.");
	}
	goTo(target, noVideo, history, true); // move in current time-line
}

// this function is used below, but factored out, because it's already way too long
static void reportProgress(Reactor& reactor, const EmuTime& targetTime, float fraction)
{
	double targetTimeDisp = (targetTime - EmuTime::zero()).toDouble();
	std::ostringstream sstr;
	sstr << "Time warping to " <<
		int(targetTimeDisp / 60) << ':' << std::setfill('0') <<
		std::setw(5) << std::setprecision(2) << std::fixed <<
		std::fmod(targetTimeDisp, 60.0);
	reactor.getCliComm().printProgress(sstr.str(), fraction);
	reactor.getDisplay().repaint();
}

void ReverseManager::goTo(
	EmuTime::param target, bool noVideo, ReverseHistory& hist,
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
		static constexpr double dur2frames = 2.0 * (313.0 * 1368.0) / (3579545.0 * 6.0);
		EmuDuration preDelta(noVideo ? 0.0 : dur2frames);
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
		ReverseChunk& chunk = it->second;
		EmuTime snapshotTime = chunk.time;
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
			// suppress messages just in case, as we're later going
			// to fast forward to the right time
			newBoard->getMSXCliComm().setSuppressMessages(true);
		} else {
			// Note: we don't (anymore) erase future snapshots
			// -- restore old snapshot --
			newBoard_ = reactor.createEmptyMotherBoard();
			newBoard = newBoard_.get();
			// suppress messages we'd get by deserializing (and
			// thus instantiating the parts of) the new board
			newBoard->getMSXCliComm().setSuppressMessages(true);
			MemInputArchive in(chunk.savestate,
					   chunk.deltaBlocks);
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
					std::make_unique<EndLogEvent>(currentTime));
			}

			// Transfer history to the new ReverseManager.
			// Also we should stop collecting in this ReverseManager,
			// and start collecting in the new one.
			auto& newManager = newBoard->getReverseManager();
			newManager.transferHistory(hist, chunk.eventCount);

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
		auto lastProgress = Timer::getTime();
		auto startMSXTime = newBoard->getCurrentTime();
		auto lastSnapshotTarget = startMSXTime;
		bool everShowedProgress = false;
		syncNewSnapshot.removeSyncPoint(); // don't schedule new snapshot takings during fast forward
		while (true) {
			auto currentTimeNewBoard = newBoard->getCurrentTime();
			auto nextSnapshotTarget = std::min(
				preTarget,
				lastSnapshotTarget + std::max(
					EmuDuration(SNAPSHOT_PERIOD),
					(preTarget - lastSnapshotTarget) / 2
					));
			auto nextTarget = std::min(nextSnapshotTarget, currentTimeNewBoard + EmuDuration::sec(1));
			newBoard->fastForward(nextTarget, true);
			if (auto now = Timer::getTime();
			    ((now - lastProgress) > 1000000) || ((currentTimeNewBoard >= preTarget) && everShowedProgress)) {
				everShowedProgress = true;
				lastProgress = now;
				auto fraction = (currentTimeNewBoard - startMSXTime).toDouble() / (preTarget - startMSXTime).toDouble();
				reportProgress(newBoard->getReactor(), targetTime, float(fraction));
			}
			// note: fastForward does not always stop at
			//       _exactly_ the requested time
			if (currentTimeNewBoard >= preTarget) break;
			if (currentTimeNewBoard >= nextSnapshotTarget) {
				// NOTE: there used to be
				//newBoard->getReactor().getEventDistributor().deliverEvents();
				// here, but that has all kinds of nasty side effects: it enables
				// processing of hotkeys, which can cause things like the machine
				// being deleted, causing a crash. TODO: find a better way to support
				// live updates of the UI whilst being in a reverse action...
				newBoard->getReverseManager().takeSnapshot(currentTimeNewBoard);
				lastSnapshotTarget = nextSnapshotTarget;
			}
		}
		// re-enable messages
		newBoard->getMSXCliComm().setSuppressMessages(false);
		// re-enable automatic snapshots
		schedule(getCurrentTime());

		// switch to the new MSXMotherBoard
		//  Note: this deletes the current MSXMotherBoard and
		//  ReverseManager. So we can't access those objects anymore.
		bool unmute = true;
		if (newBoard_) {
			unmute = false;
			reactor.replaceBoard(motherBoard, std::move(newBoard_));
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
	// Transfer view only mode
	const auto& oldDistributor = motherBoard.getStateChangeDistributor();
	      auto& newDistributor = newBoard   .getStateChangeDistributor();
	newDistributor.setViewOnlyMode(oldDistributor.isViewOnlyMode());

	// transfer keyboard state
	auto& newManager = newBoard.getReverseManager();
	if (auto* newKeyb = newManager.motherBoard.getKeyboard()) {
		if (const auto* oldKeyb = motherBoard.getKeyboard()) {
			newKeyb->transferHostKeyMatrix(*oldKeyb);
		}
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
	Interpreter& interp, std::span<const TclObject> tokens, TclObject& result)
{
	const auto& chunks = history.chunks;
	if (chunks.empty()) {
		throw CommandException("No recording...");
	}

	std::string_view filenameArg;
	int maxNofExtraSnapshots = MAX_NOF_SNAPSHOTS;
	std::array info = {valueArg("-maxnofextrasnapshots", maxNofExtraSnapshots)};
	auto args = parseTclArgs(interp, tokens.subspan(2), info);
	switch (args.size()) {
		case 0: break; // nothing
		case 1: filenameArg = args[0].getString(); break;
		default: throw SyntaxError();
	}
	if (maxNofExtraSnapshots < 0) {
		throw CommandException("Maximum number of snapshots should be at least 0");
	}

	auto filename = FileOperations::parseCommandFileArgument(
		filenameArg, REPLAY_DIR, "openmsx", REPLAY_EXTENSION);

	auto& reactor = motherBoard.getReactor();
	Replay replay(reactor);
	replay.reRecordCount = reRecordCount;

	// store current time (possibly somewhere in the middle of the timeline)
	// so that on load we can go back there
	replay.currentTime = getCurrentTime();

	// restore first snapshot to be able to serialize it to a file
	auto initialBoard = reactor.createEmptyMotherBoard();
	MemInputArchive in(begin(chunks)->second.savestate,
			   begin(chunks)->second.deltaBlocks);
	in.serialize("machine", *initialBoard);
	replay.motherBoards.push_back(std::move(initialBoard));

	if (maxNofExtraSnapshots > 0) {
		// determine which extra snapshots to put in the replay
		const auto& startTime = begin(chunks)->second.time;
		// for the end time, try to take MAX_DIST_1_BEFORE_LAST_SNAPSHOT
		// seconds before the normal end time so that we get an extra snapshot
		// at that point, which is comfortable if you want to reverse from the
		// last snapshot after loading the replay.
		const auto& lastChunkTime = rbegin(chunks)->second.time;
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
					MemInputArchive in2(it->second.savestate,
							    it->second.deltaBlocks);
					in2.serialize("machine", *board);
					replay.motherBoards.push_back(std::move(board));
					lastAddedIt = it;
				}
				++it;
				while (it != end(chunks) && it->second.time > nextPartitionEnd) {
					nextPartitionEnd += partitionLength;
				}
			}
		}
		assert(lastAddedIt == std::prev(end(chunks))); // last snapshot must be included
	}

	// add sentinel when there isn't one yet
	bool addSentinel = history.events.empty() ||
		!dynamic_cast<EndLogEvent*>(history.events.back().get());
	if (addSentinel) {
		/// make sure the replay log ends with a EndLogEvent
		history.events.push_back(std::make_unique<EndLogEvent>(
			getCurrentTime()));
	}
	try {
		XmlOutputArchive out(filename);
		replay.events = &history.events;
		out.serialize("replay", replay);
		out.close();
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

	result = tmpStrCat("Saved replay to ", filename);
}

void ReverseManager::loadReplay(
	Interpreter& interp, std::span<const TclObject> tokens, TclObject& result)
{
	bool enableViewOnly = false;
	std::optional<TclObject> where;
	std::array info = {
		flagArg("-viewonly", enableViewOnly),
		valueArg("-goto", where),
	};
	auto arguments = parseTclArgs(interp, tokens.subspan(2), info);
	if (arguments.size() != 1) throw SyntaxError();

	// resolve the filename
	auto context = userDataFileContext(REPLAY_DIR);
	std::string fileNameArg(arguments[0].getString());
	std::string filename;
	try {
		// Try filename as typed by user.
		filename = context.resolve(fileNameArg);
	} catch (MSXException& /*e1*/) { try {
		// Not found, try adding the normal extension
		filename = context.resolve(tmpStrCat(fileNameArg, REPLAY_EXTENSION));
	} catch (MSXException& e2) { try {
		// Again not found, try adding '.gz'.
		// (this is for backwards compatibility).
		filename = context.resolve(tmpStrCat(fileNameArg, ".gz"));
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
		throw CommandException("Cannot load replay, bad file format: ",
		                       e.getMessage());
	} catch (MSXException& e) {
		throw CommandException("Cannot load replay: ", e.getMessage());
	}

	// get destination time index
	auto destination = EmuTime::zero();
	if (!where || (*where == "begin")) {
		destination = EmuTime::zero();
	} else if (*where == "end") {
		destination = EmuTime::infinity();
	} else if (*where == "savetime") {
		destination = replay.currentTime;
	} else {
		destination += EmuDuration(where->getDouble(interp));
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
	for (const auto& m : replay.motherBoards) {
		ReverseChunk newChunk;
		newChunk.time = m->getCurrentTime();

		MemOutputArchive out(newHistory.lastDeltaBlocks,
		                     newChunk.deltaBlocks, false);
		out.serialize("machine", *m);
		newChunk.savestate = std::move(out).releaseBuffer();

		// update replayIdx
		// TODO: should we use <= instead??
		while (replayIdx < newEvents.size() &&
		       (newEvents[replayIdx]->getTime() < newChunk.time)) {
			replayIdx++;
		}
		newChunk.eventCount = replayIdx;

		newHistory.chunks[newHistory.getNextSeqNum(newChunk.time)] =
			std::move(newChunk);
	}

	// Note: until this point we didn't make any changes to the current
	// ReverseManager/MSXMotherBoard yet
	reRecordCount = newReverseManager.reRecordCount;
	bool noVideo = false;
	goTo(destination, noVideo, newHistory, false); // move to different time-line

	result = tmpStrCat("Loaded replay from ", filename);
}

void ReverseManager::transferHistory(ReverseHistory& oldHistory,
                                     unsigned oldEventCount)
{
	assert(!isCollecting());
	assert(history.chunks.empty());

	// 'ids' for old and new serialize blobs don't match, so cleanup old cache
	oldHistory.lastDeltaBlocks.clear();

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
	eventDistributor.distributeEvent(TakeReverseSnapshotEvent());
}

void ReverseManager::execInputEvent()
{
	const auto& event = *history.events[replayIndex];
	try {
		// deliver current event at current time
		motherBoard.getStateChangeDistributor().distributeReplay(event);
	} catch (MSXException&) {
		// can throw in case we replay a command that fails
		// ignore
	}
	if (!dynamic_cast<const EndLogEvent*>(&event)) {
		++replayIndex;
		replayNextEvent();
	} else {
		signalStopReplay(event.getTime());
		assert(!isReplaying());
	}
}

bool ReverseManager::signalEvent(const Event& event)
{
	(void)event;
	assert(getType(event) == EventType::TAKE_REVERSE_SNAPSHOT);

	// This event is send to all MSX machines, make sure it's actually this
	// machine that requested the snapshot.
	if (pendingTakeSnapshot) {
		pendingTakeSnapshot = false;
		takeSnapshot(getCurrentTime());
		// schedule creation of next snapshot
		schedule(getCurrentTime());
	}
	return false;
}

unsigned ReverseManager::ReverseHistory::getNextSeqNum(EmuTime::param time) const
{
	if (chunks.empty()) {
		return 0;
	}
	const auto& startTime = begin(chunks)->second.time;
	double duration = (time - startTime).toDouble();
	return narrow<unsigned>(lrint(duration / SNAPSHOT_PERIOD));
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
	ReverseChunk& newChunk = history.chunks[seqNum];
	newChunk.deltaBlocks.clear();
	MemOutputArchive out(history.lastDeltaBlocks, newChunk.deltaBlocks, true);
	out.serialize("machine", motherBoard);
	newChunk.time = time;
	newChunk.savestate = std::move(out).releaseBuffer();
	newChunk.eventCount = replayIndex;
}

void ReverseManager::replayNextEvent()
{
	// schedule next event at its own time
	assert(replayIndex < history.events.size());
	syncInputEvent.setSyncPoint(history.events[replayIndex]->getTime());
}

void ReverseManager::signalStopReplay(EmuTime::param time)
{
	motherBoard.getStateChangeDistributor().stopReplay(time);
	// this is needed to prevent a reRecordCount increase
	// due to this action ending the replay
	reRecordCount--;
}

void ReverseManager::stopReplay(EmuTime::param time) noexcept
{
	if (isReplaying()) {
		// if we're replaying, stop it and erase remainder of event log
		syncInputEvent.removeSyncPoint();
		Events& events = history.events;
		events.erase(begin(events) + replayIndex, end(events));
		// search snapshots that are newer than 'time' and erase them
		auto it = ranges::find_if(history.chunks, [&](auto& p) {
			return p.second.time > time;
		});
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

void ReverseManager::ReverseCmd::execute(std::span<const TclObject> tokens, TclObject& result)
{
	checkNumArgs(tokens, AtLeast{2}, "subcommand ?arg ...?");
	auto& manager = OUTER(ReverseManager, reverseCmd);
	auto& interp = getInterpreter();
	executeSubCommand(tokens[1].getString(),
		"start",      [&]{ manager.start(); },
		"stop",       [&]{ manager.stop(); },
		"status",     [&]{ manager.status(result); },
		"debug",      [&]{ manager.debugInfo(result); },
		"goback",     [&]{ manager.goBack(tokens); },
		"goto",       [&]{ manager.goTo(tokens); },
		"savereplay", [&]{ manager.saveReplay(interp, tokens, result); },
		"loadreplay", [&]{ manager.loadReplay(interp, tokens, result); },
		"viewonlymode", [&]{
			auto& distributor = manager.motherBoard.getStateChangeDistributor();
			switch (tokens.size()) {
			case 2:
				result = distributor.isViewOnlyMode();
				break;
			case 3:
				distributor.setViewOnlyMode(tokens[2].getBoolean(interp));
				break;
			default:
				throw SyntaxError();
			}},
		"truncatereplay", [&] {
			if (manager.isReplaying()) {
				manager.signalStopReplay(manager.getCurrentTime());
			}});
}

std::string ReverseManager::ReverseCmd::help(std::span<const TclObject> /*tokens*/) const
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

void ReverseManager::ReverseCmd::tabCompletion(std::vector<std::string>& tokens) const
{
	using namespace std::literals;
	if (tokens.size() == 2) {
		static constexpr std::array subCommands = {
			"start"sv, "stop"sv, "status"sv, "goback"sv, "goto"sv,
			"savereplay"sv, "loadreplay"sv, "viewonlymode"sv,
			"truncatereplay"sv,
		};
		completeString(tokens, subCommands);
	} else if ((tokens.size() == 3) || (tokens[1] == "loadreplay")) {
		if (tokens[1] == one_of("loadreplay", "savereplay")) {
			static constexpr std::array cmds = {"-goto"sv, "-viewonly"sv};
			completeFileName(tokens, userDataFileContext(REPLAY_DIR),
				(tokens[1] == "loadreplay") ? cmds : std::span<const std::string_view>{});
		} else if (tokens[1] == "viewonlymode") {
			static constexpr std::array options = {"true"sv, "false"sv};
			completeString(tokens, options);
		}
	}
}

} // namespace openmsx
