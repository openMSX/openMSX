#ifndef DEBUGGER_HH
#define DEBUGGER_HH

#include "Probe.hh"
#include "RecordedCommand.hh"
#include "WatchPoint.hh"
#include "hash_map.hh"
#include "string_view.hh"
#include "outer.hh"
#include "xxhash.hh"
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
	void unregisterDebuggable (string_view name, Debuggable& debuggable);
	Debuggable* findDebuggable(string_view name);

	void registerProbe  (ProbeBase& probe);
	void unregisterProbe(ProbeBase& probe);
	ProbeBase* findProbe(string_view name);

	void removeProbeBreakPoint(ProbeBreakPoint& bp);
	void setCPU(MSXCPU* cpu_) { cpu = cpu_; }

	void transfer(Debugger& other);

	MSXMotherBoard& getMotherBoard() { return motherBoard; }

private:
	Debuggable& getDebuggable(string_view name);
	ProbeBase& getProbe(string_view name);

	unsigned insertProbeBreakPoint(
		TclObject command, TclObject condition,
		ProbeBase& probe, unsigned newId = -1);
	void removeProbeBreakPoint(string_view name);

	unsigned setWatchPoint(TclObject command, TclObject condition,
	                       WatchPoint::Type type,
	                       unsigned beginAddr, unsigned endAddr,
	                       unsigned newId = -1);

	MSXMotherBoard& motherBoard;

	class Cmd final : public RecordedCommand {
	public:
		Cmd(CommandController& commandController,
		    StateChangeDistributor& stateChangeDistributor,
		    Scheduler& scheduler);
		bool needRecord(array_ref<TclObject> tokens) const override;
		void execute(array_ref<TclObject> tokens,
			     TclObject& result, EmuTime::param time) override;
		std::string help(const std::vector<std::string>& tokens) const override;
		void tabCompletion(std::vector<std::string>& tokens) const override;

	private:
		      Debugger& debugger()       { return OUTER(Debugger, cmd); }
		const Debugger& debugger() const { return OUTER(Debugger, cmd); }
		void list(TclObject& result);
		void desc(array_ref<TclObject> tokens, TclObject& result);
		void size(array_ref<TclObject> tokens, TclObject& result);
		void read(array_ref<TclObject> tokens, TclObject& result);
		void readBlock(array_ref<TclObject> tokens, TclObject& result);
		void write(array_ref<TclObject> tokens, TclObject& result);
		void writeBlock(array_ref<TclObject> tokens, TclObject& result);
		void setBreakPoint(array_ref<TclObject> tokens, TclObject& result);
		void removeBreakPoint(array_ref<TclObject> tokens, TclObject& result);
		void listBreakPoints(array_ref<TclObject> tokens, TclObject& result);
		std::vector<std::string> getBreakPointIds() const;
		std::vector<std::string> getWatchPointIds() const;
		std::vector<std::string> getConditionIds() const;
		void setWatchPoint(array_ref<TclObject> tokens, TclObject& result);
		void removeWatchPoint(array_ref<TclObject> tokens, TclObject& result);
		void listWatchPoints(array_ref<TclObject> tokens, TclObject& result);
		void setCondition(array_ref<TclObject> tokens, TclObject& result);
		void removeCondition(array_ref<TclObject> tokens, TclObject& result);
		void listConditions(array_ref<TclObject> tokens, TclObject& result);
		void probe(array_ref<TclObject> tokens, TclObject& result);
		void probeList(array_ref<TclObject> tokens, TclObject& result);
		void probeDesc(array_ref<TclObject> tokens, TclObject& result);
		void probeRead(array_ref<TclObject> tokens, TclObject& result);
		void probeSetBreakPoint(array_ref<TclObject> tokens, TclObject& result);
		void probeRemoveBreakPoint(array_ref<TclObject> tokens, TclObject& result);
		void probeListBreakPoints(array_ref<TclObject> tokens, TclObject& result);
	} cmd;

	struct NameFromProbe {
		const std::string& operator()(const ProbeBase* p) const {
			return p->getName();
		}
	};

	hash_map<std::string, Debuggable*, XXHasher> debuggables;
	hash_set<ProbeBase*, NameFromProbe, XXHasher>  probes;
	using ProbeBreakPoints = std::vector<std::unique_ptr<ProbeBreakPoint>>;
	ProbeBreakPoints probeBreakPoints; // unordered
	MSXCPU* cpu;
};

} // namespace openmsx

#endif
