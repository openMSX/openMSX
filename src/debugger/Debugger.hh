// $Id$

#ifndef DEBUGGER_HH
#define DEBUGGER_HH

#include <map>
#include <set>
#include <string>
#include <memory>

namespace openmsx {

class Debuggable;
class CommandController;
class MSXCPU;
class DebugCmd;

class Debugger
{
public:
	explicit Debugger(CommandController& commandController);
	~Debugger();

	void registerDebuggable  (const std::string& name, Debuggable& interface);
	void unregisterDebuggable(const std::string& name, Debuggable& interface);
	Debuggable* findDebuggable(const std::string& name);

	void setCPU(MSXCPU* cpu);

private:
	Debuggable* getDebuggable(const std::string& name);
	void getDebuggables(std::set<std::string>& result) const;

	friend class DebugCmd;
	const std::auto_ptr<DebugCmd> debugCmd;

	std::map<std::string, Debuggable*> debuggables;
	MSXCPU* cpu;
};

} // namespace openmsx

#endif
