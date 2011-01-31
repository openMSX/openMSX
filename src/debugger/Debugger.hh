// $Id$

#ifndef DEBUGGER_HH
#define DEBUGGER_HH

#include "WatchPoint.hh"
#include "noncopyable.hh"
#include <vector>
#include <map>
#include <set>
#include <string>
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

	void registerDebuggable  (const std::string& name, Debuggable& interface);
	void unregisterDebuggable(const std::string& name, Debuggable& interface);
	Debuggable* findDebuggable(const std::string& name);

	void registerProbe  (const std::string& name, ProbeBase& probe);
	void unregisterProbe(const std::string& name, ProbeBase& probe);
	ProbeBase* findProbe(const std::string& name);

	void removeProbeBreakPoint(ProbeBreakPoint& bp);
	void setCPU(MSXCPU* cpu);

	void transfer(Debugger& other);

private:
	Debuggable& getDebuggable(const std::string& name);
	void getDebuggables(std::set<std::string>& result) const;

	ProbeBase& getProbe(const std::string& name);
	void getProbes(std::set<std::string>& result) const;

	unsigned insertProbeBreakPoint(
		std::auto_ptr<TclObject> command,
		std::auto_ptr<TclObject> condition,
		ProbeBase& probe, unsigned newId = -1);
	void removeProbeBreakPoint(const std::string& name);

	unsigned setWatchPoint(std::auto_ptr<TclObject> command,
	                       std::auto_ptr<TclObject> condition,
	                       WatchPoint::Type type,
	                       unsigned beginAddr, unsigned endAddr,
	                       unsigned newId = -1);

	MSXMotherBoard& motherBoard;
	friend class DebugCmd;
	const std::auto_ptr<DebugCmd> debugCmd;

	typedef std::map<std::string, Debuggable*> Debuggables;
	typedef std::map<std::string, ProbeBase*>  Probes;
	typedef std::vector<ProbeBreakPoint*> ProbeBreakPoints;
	Debuggables debuggables;
	Probes probes;
	ProbeBreakPoints probeBreakPoints;
	MSXCPU* cpu;
};

} // namespace openmsx

#endif
