// $Id$

#include <map>
#include <set>
#include <string>
#include "Command.hh"

using std::map;
using std::set;
using std::string;

namespace openmsx {

class Debuggable;
class CommandController;
class MSXCPU;

class Debugger
{
public:
	static Debugger& instance();

	void registerDebuggable  (const string& name, Debuggable& interface);
	void unregisterDebuggable(const string& name, Debuggable& interface);

	void setCPU(MSXCPU* cpu);

private:
	Debugger();
	~Debugger();
	
	Debuggable* getDebuggable(const string& name);
	void getDebuggables(set<string>& result) const;

	class DebugCmd : public Command {
	public:
		DebugCmd(Debugger& parent);
		virtual string execute(const vector<string>& tokens)
			throw(CommandException);
		virtual string help(const vector<string>& tokens) const
			throw();
		virtual void tabCompletion(vector<string>& tokens) const
			throw();

	private:
		string list();
		string desc(const vector<string>& tokens);
		string size(const vector<string>& tokens);
		string read(const vector<string>& tokens);
		string write(const vector<string>& tokens);
		
		Debugger& parent;
	} debugCmd;
	
	map<string, Debuggable*> debuggables;
	CommandController& commandController;
	MSXCPU* cpu;
};

} // namespace openmsx

