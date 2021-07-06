#ifndef REVERSEMANGER_HH
#define REVERSEMANGER_HH

#include "Schedulable.hh"
#include "EventListener.hh"
#include "StateChangeListener.hh"
#include "Command.hh"
#include "EmuTime.hh"
#include "MemBuffer.hh"
#include "DeltaBlock.hh"
#include "span.hh"
#include "outer.hh"
#include <vector>
#include <map>
#include <memory>
#include <cstdint>

namespace openmsx {

class MSXMotherBoard;
class Keyboard;
class EventDelay;
class EventDistributor;
class TclObject;
class Interpreter;

class ReverseManager final : private EventListener, private StateChangeRecorder
{
public:
	explicit ReverseManager(MSXMotherBoard& motherBoard);
	~ReverseManager();

	// Keyboard is special because we need to transfer the host keyboard
	// state on 'reverse goto' to be able to resynchronize when replay
	// stops. See Keyboard::transferHostKeyMatrix() for more info.
	void registerKeyboard(Keyboard& keyboard_) {
		keyboard = &keyboard_;
	}

	// To not loose any events we need to flush delayed events before
	// switching machine. See comments in goTo() for more info.
	void registerEventDelay(EventDelay& eventDelay_) {
		eventDelay = &eventDelay_;
	}

	// Should only be used by MSXMotherBoard to be able to transfer
	// reRecordCount to ReverseManager for version 2 of MSXMotherBoard
	// serializers.
	void setReRecordCount(unsigned count) {
		reRecordCount = count;
	}

	[[nodiscard]] bool isReplaying() const override;

private:
	struct ReverseChunk {
		ReverseChunk() : time(EmuTime::zero()) {}

		EmuTime time;
		std::vector<std::shared_ptr<DeltaBlock>> deltaBlocks;
		MemBuffer<uint8_t> savestate;
		size_t size;

		// Number of recorded events (or replay index) when this
		// snapshot was created. So when going back replay should
		// start at this index.
		unsigned eventCount;
	};
	using Chunks = std::map<unsigned, ReverseChunk>;
	using Events = std::vector<std::shared_ptr<StateChange>>;

	struct ReverseHistory {
		void swap(ReverseHistory& other) noexcept;
		void clear();
		[[nodiscard]] unsigned getNextSeqNum(EmuTime::param time) const;

		Chunks chunks;
		Events events;
		LastDeltaBlocks lastDeltaBlocks;
	};

	[[nodiscard]] bool isCollecting() const { return collecting; }

	void start();
	void stop();
	void status(TclObject& result) const;
	void debugInfo(TclObject& result) const;
	void goBack(span<const TclObject> tokens);
	void goTo(span<const TclObject> tokens);
	void saveReplay(Interpreter& interp,
	                span<const TclObject> tokens, TclObject& result);
	void loadReplay(Interpreter& interp,
	                span<const TclObject> tokens, TclObject& result);

	void signalStopReplay(EmuTime::param time);
	[[nodiscard]] EmuTime::param getEndTime(const ReverseHistory& history) const;
	void goTo(EmuTime::param targetTime, bool novideo);
	void goTo(EmuTime::param targetTime, bool novideo,
	          ReverseHistory& history, bool sameTimeLine);
	void transferHistory(ReverseHistory& oldHistory,
	                     unsigned oldEventCount);
	void transferState(MSXMotherBoard& newBoard);
	void takeSnapshot(EmuTime::param time);
	void schedule(EmuTime::param time);
	void replayNextEvent();
	template<unsigned N> void dropOldSnapshots(unsigned count);

	// Schedulable
	struct SyncNewSnapshot final : Schedulable {
		friend class ReverseManager;
		explicit SyncNewSnapshot(Scheduler& s) : Schedulable(s) {}
		void executeUntil(EmuTime::param /*time*/) override {
			auto& rm = OUTER(ReverseManager, syncNewSnapshot);
			rm.execNewSnapshot();
		}
	} syncNewSnapshot;
	struct SyncInputEvent final : Schedulable {
		friend class ReverseManager;
		explicit SyncInputEvent(Scheduler& s) : Schedulable(s) {}
		void executeUntil(EmuTime::param /*time*/) override {
			auto& rm = OUTER(ReverseManager, syncInputEvent);
			rm.execInputEvent();
		}
	} syncInputEvent;

	void execNewSnapshot();
	void execInputEvent();
	[[nodiscard]] EmuTime::param getCurrentTime() const { return syncNewSnapshot.getCurrentTime(); }

	// EventListener
	int signalEvent(const Event& event) noexcept override;

	// StateChangeRecorder
	void record(const std::shared_ptr<StateChange>& event) override;
	void stopReplay(EmuTime::param time) noexcept override;

private:
	MSXMotherBoard& motherBoard;
	EventDistributor& eventDistributor;

	struct ReverseCmd final : Command {
		explicit ReverseCmd(CommandController& controller);
		void execute(span<const TclObject> tokens, TclObject& result) override;
		[[nodiscard]] std::string help(span<const TclObject> tokens) const override;
		void tabCompletion(std::vector<std::string>& tokens) const override;
	} reverseCmd;

	Keyboard* keyboard;
	EventDelay* eventDelay;
	ReverseHistory history;
	unsigned replayIndex;
	bool collecting;
	bool pendingTakeSnapshot;

	unsigned reRecordCount;

	friend struct Replay;
};

} // namespace openmsx

#endif
