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
	Debuggable* findDebuggable(std::string_view name);

	void registerProbe  (ProbeBase& probe);
	void unregisterProbe(ProbeBase& probe);
	ProbeBase* findProbe(std::string_view name);

	void removeProbeBreakPoint(ProbeBreakPoint& bp);
	void setCPU(MSXCPU* cpu_) { cpu = cpu_; }

	void transfer(Debugger& other);

	MSXMotherBoard& getMotherBoard() { return motherBoard; }

private:
	Debuggable& getDebuggable(std::string_view name);
	ProbeBase& getProbe(std::string_view name);

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
		bool needRecord(span<const TclObject> tokens) const override;
		void execute(span<const TclObject> tokens,
			     TclObject& result, EmuTime::param time) override;
		std::string help(const std::vector<std::string>& tokens) const override;
		void tabCompletion(std::vector<std::string>& tokens) const override;

	private:
		      Debugger& debugger()       { return OUTER(Debugger, cmd); }
		const Debugger& debugger() const { return OUTER(Debugger, cmd); }
		void list(TclObject& result);
		void desc(span<const TclObject> tokens, TclObject& result);
		void size(span<const TclObject> tokens, TclObject& result);
		void read(span<const TclObject> tokens, TclObject& result);
		void readBlock(span<const TclObject> tokens, TclObject& result);
		void write(span<const TclObject> tokens, TclObject& result);
		void writeBlock(span<const TclObject> tokens, TclObject& result);
		void setBreakPoint(span<const TclObject> tokens, TclObject& result);
		void removeBreakPoint(span<const TclObject> tokens, TclObject& result);
		void listBreakPoints(span<const TclObject> tokens, TclObject& result);
		std::vector<std::string> getBreakPointIds() const;
		std::vector<std::string> getWatchPointIds() const;
		std::vector<std::string> getConditionIds() const;
		void setWatchPoint(span<const TclObject> tokens, TclObject& result);
		void removeWatchPoint(span<const TclObject> tokens, TclObject& result);
		void listWatchPoints(span<const TclObject> tokens, TclObject& result);
		void setCondition(span<const TclObject> tokens, TclObject& result);
		void removeCondition(span<const TclObject> tokens, TclObject& result);
		void listConditions(span<const TclObject> tokens, TclObject& result);
		void probe(span<const TclObject> tokens, TclObject& result);
		void probeList(span<const TclObject> tokens, TclObject& result);
		void probeDesc(span<const TclObject> tokens, TclObject& result);
		void probeRead(span<const TclObject> tokens, TclObject& result);
		void probeSetBreakPoint(span<const TclObject> tokens, TclObject& result);
		void probeRemoveBreakPoint(span<const TclObject> tokens, TclObject& result);
		void probeListBreakPoints(span<const TclObject> tokens, TclObject& result);
	} cmd;

	struct NameFromProbe {
		const std::string& operator()(const ProbeBase* p) const {
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
