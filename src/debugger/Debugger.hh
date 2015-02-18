#ifndef DEBUGGER_HH
#define DEBUGGER_HH

#include "RecordedCommand.hh"
#include "WatchPoint.hh"
#include "StringMap.hh"
#include "string_ref.hh"
#include "noncopyable.hh"
#include <vector>
#include <memory>

namespace openmsx {

class MSXMotherBoard;
class Debuggable;
class ProbeBase;
class ProbeBreakPoint;
class MSXCPU;

class Debugger : private noncopyable
{
public:
	explicit Debugger(MSXMotherBoard& motherBoard);
	~Debugger();

	void registerDebuggable   (string_ref name, Debuggable& interface);
	void unregisterDebuggable (string_ref name, Debuggable& interface);
	Debuggable* findDebuggable(string_ref name);

	void registerProbe  (string_ref name, ProbeBase& probe);
	void unregisterProbe(string_ref name, ProbeBase& probe);
	ProbeBase* findProbe(string_ref name);

	void removeProbeBreakPoint(ProbeBreakPoint& bp);
	void setCPU(MSXCPU* cpu_) { cpu = cpu_; }

	void transfer(Debugger& other);

private:
	Debuggable& getDebuggable(string_ref name);
	ProbeBase& getProbe(string_ref name);

	unsigned insertProbeBreakPoint(
		TclObject command, TclObject condition,
		ProbeBase& probe, unsigned newId = -1);
	void removeProbeBreakPoint(string_ref name);

	unsigned setWatchPoint(TclObject command, TclObject condition,
	                       WatchPoint::Type type,
	                       unsigned beginAddr, unsigned endAddr,
	                       unsigned newId = -1);

	MSXMotherBoard& motherBoard;

	class Cmd final : public RecordedCommand {
	public:
		Cmd(CommandController& commandController,
		    StateChangeDistributor& stateChangeDistributor,
		    Scheduler& scheduler, GlobalCliComm& cliComm,
		    Debugger& debugger);
		bool needRecord(array_ref<TclObject> tokens) const override;
		void execute(array_ref<TclObject> tokens,
			     TclObject& result, EmuTime::param time) override;
		std::string help(const std::vector<std::string>& tokens) const override;
		void tabCompletion(std::vector<std::string>& tokens) const override;

	private:
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

		GlobalCliComm& cliComm;
		Debugger& debugger;
	} cmd;

	StringMap<Debuggable*> debuggables;
	StringMap<ProbeBase*>  probes;
	using ProbeBreakPoints = std::vector<std::unique_ptr<ProbeBreakPoint>>;
	ProbeBreakPoints probeBreakPoints;
	MSXCPU* cpu;
};

} // namespace openmsx

#endif
