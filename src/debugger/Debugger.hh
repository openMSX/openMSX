#ifndef DEBUGGER_HH
#define DEBUGGER_HH

#include "Probe.hh"
#include "RecordedCommand.hh"
#include "WatchPoint.hh"
#include "hash_map.hh"
#include "outer.hh"
#include "xxhash.hh"
#include <string_view>
#include <vector>
#include <memory>

namespace openmsx {

class MSXMotherBoard;
class Debuggable;
class ProbeBase;
class ProbeBreakPoint;
class MSXCPU;

class Debugger
{
public:
	Debugger(const Debugger&) = delete;
	Debugger& operator=(const Debugger&) = delete;

	explicit Debugger(MSXMotherBoard& motherBoard);
	~Debugger();

	void registerDebuggable   (std::string name, Debuggable& debuggable);
	void unregisterDebuggable (std::string_view name, Debuggable& debuggable);
	[[nodiscard]] Debuggable* findDebuggable(std::string_view name);

	void registerProbe  (ProbeBase& probe);
	void unregisterProbe(ProbeBase& probe);
	[[nodiscard]] ProbeBase* findProbe(std::string_view name);

	void removeProbeBreakPoint(ProbeBreakPoint& bp);
	void setCPU(MSXCPU* cpu_) { cpu = cpu_; }

	void transfer(Debugger& other);

	[[nodiscard]] MSXMotherBoard& getMotherBoard() { return motherBoard; }

private:
	[[nodiscard]] Debuggable& getDebuggable(std::string_view name);
	[[nodiscard]] ProbeBase& getProbe(std::string_view name);

	unsigned insertProbeBreakPoint(
		TclObject command, TclObject condition,
		ProbeBase& probe, bool once, unsigned newId = -1);
	void removeProbeBreakPoint(std::string_view name);

	unsigned setWatchPoint(TclObject command, TclObject condition,
	                       WatchPoint::Type type,
	                       unsigned beginAddr, unsigned endAddr,
	                       bool once, unsigned newId = -1);

	MSXMotherBoard& motherBoard;

	class Cmd final : public RecordedCommand {
	public:
		Cmd(CommandController& commandController,
		    StateChangeDistributor& stateChangeDistributor,
		    Scheduler& scheduler);
		[[nodiscard]] bool needRecord(std::span<const TclObject> tokens) const override;
		void execute(std::span<const TclObject> tokens,
			     TclObject& result, EmuTime::param time) override;
		[[nodiscard]] std::string help(std::span<const TclObject> tokens) const override;
		void tabCompletion(std::vector<std::string>& tokens) const override;

	private:
		[[nodiscard]]       Debugger& debugger()       { return OUTER(Debugger, cmd); }
		[[nodiscard]] const Debugger& debugger() const { return OUTER(Debugger, cmd); }
		void list(TclObject& result);
		void desc(std::span<const TclObject> tokens, TclObject& result);
		void size(std::span<const TclObject> tokens, TclObject& result);
		void read(std::span<const TclObject> tokens, TclObject& result);
		void readBlock(std::span<const TclObject> tokens, TclObject& result);
		void write(std::span<const TclObject> tokens, TclObject& result);
		void writeBlock(std::span<const TclObject> tokens, TclObject& result);
		void setBreakPoint(std::span<const TclObject> tokens, TclObject& result);
		void removeBreakPoint(std::span<const TclObject> tokens, TclObject& result);
		void listBreakPoints(std::span<const TclObject> tokens, TclObject& result);
		[[nodiscard]] std::vector<std::string> getBreakPointIds() const;
		[[nodiscard]] std::vector<std::string> getWatchPointIds() const;
		[[nodiscard]] std::vector<std::string> getConditionIds() const;
		void setWatchPoint(std::span<const TclObject> tokens, TclObject& result);
		void removeWatchPoint(std::span<const TclObject> tokens, TclObject& result);
		void listWatchPoints(std::span<const TclObject> tokens, TclObject& result);
		void setCondition(std::span<const TclObject> tokens, TclObject& result);
		void removeCondition(std::span<const TclObject> tokens, TclObject& result);
		void listConditions(std::span<const TclObject> tokens, TclObject& result);
		void probe(std::span<const TclObject> tokens, TclObject& result);
		void probeList(std::span<const TclObject> tokens, TclObject& result);
		void probeDesc(std::span<const TclObject> tokens, TclObject& result);
		void probeRead(std::span<const TclObject> tokens, TclObject& result);
		void probeSetBreakPoint(std::span<const TclObject> tokens, TclObject& result);
		void probeRemoveBreakPoint(std::span<const TclObject> tokens, TclObject& result);
		void probeListBreakPoints(std::span<const TclObject> tokens, TclObject& result);
	} cmd;

	struct NameFromProbe {
		[[nodiscard]] const std::string& operator()(const ProbeBase* p) const {
			return p->getName();
		}
	};

	hash_map<std::string, Debuggable*, XXHasher> debuggables;
	hash_set<ProbeBase*, NameFromProbe, XXHasher> probes;
	std::vector<std::unique_ptr<ProbeBreakPoint>> probeBreakPoints; // unordered
	MSXCPU* cpu = nullptr;
};

} // namespace openmsx

#endif
