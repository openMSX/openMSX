// $Id$

#ifndef DEBUGGER_HH
#define DEBUGGER_HH

#include "noncopyable.hh"
#include <map>
#include <set>
#include <string>
#include <memory>

namespace openmsx {

class MSXMotherBoard;
class Debuggable;
class ProbeBase;
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

	void setCPU(MSXCPU* cpu);

private:
	Debuggable& getDebuggable(const std::string& name);
	void getDebuggables(std::set<std::string>& result) const;

	ProbeBase& getProbe(const std::string& name);
	void getProbes(std::set<std::string>& result) const;

	MSXMotherBoard& motherBoard;
	friend class DebugCmd;
	const std::auto_ptr<DebugCmd> debugCmd;

	typedef std::map<std::string, Debuggable*> Debuggables;
	typedef std::map<std::string, ProbeBase*>  Probes;
	Debuggables debuggables;
	Probes      probes;
	MSXCPU* cpu;
};

} // namespace openmsx

#endif
