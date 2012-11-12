// $Id$

#ifndef DEBUGGER_HH
#define DEBUGGER_HH

#include "WatchPoint.hh"
#include "StringMap.hh"
#include "string_ref.hh"
#include "noncopyable.hh"
#include <vector>
#include <set>
#include <memory>

namespace openmsx {

class MSXMotherBoard;
class Debuggable;
class ProbeBase;
class ProbeBreakPoint;
class MSXCPU;
class DebugCmd;

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
	void setCPU(MSXCPU* cpu);

	void transfer(Debugger& other);

private:
	Debuggable& getDebuggable(string_ref name);
	std::set<std::string> getDebuggables() const;

	ProbeBase& getProbe(string_ref name);
	std::set<std::string> getProbes() const;

	unsigned insertProbeBreakPoint(
		TclObject command, TclObject condition,
		ProbeBase& probe, unsigned newId = -1);
	void removeProbeBreakPoint(string_ref name);

	unsigned setWatchPoint(TclObject command, TclObject condition,
	                       WatchPoint::Type type,
	                       unsigned beginAddr, unsigned endAddr,
	                       unsigned newId = -1);

	MSXMotherBoard& motherBoard;
	friend class DebugCmd;
	const std::unique_ptr<DebugCmd> debugCmd;

	typedef StringMap<Debuggable*> Debuggables;
	typedef StringMap<ProbeBase*>  Probes;
	typedef std::vector<std::unique_ptr<ProbeBreakPoint>> ProbeBreakPoints;
	Debuggables debuggables;
	Probes probes;
	ProbeBreakPoints probeBreakPoints;
	MSXCPU* cpu;
};

} // namespace openmsx

#endif
