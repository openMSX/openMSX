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
		virtual void execute(const vector<CommandArgument>& tokens,
		                     CommandArgument& result);
		virtual string help(const vector<string>& tokens) const;
		virtual void tabCompletion(vector<string>& tokens) const;

	private:
		void list(CommandArgument& result);
		void desc(const vector<CommandArgument>& tokens,
		          CommandArgument& result);
		void size(const vector<CommandArgument>& tokens,
		          CommandArgument& result);
		void read(const vector<CommandArgument>& tokens,
		          CommandArgument& result);
		void readBlock(const vector<CommandArgument>& tokens,
		               CommandArgument& result);
		void write(const vector<CommandArgument>& tokens,
		           CommandArgument& result);
		void writeBlock(const vector<CommandArgument>& tokens,
		                CommandArgument& result);
		
		Debugger& parent;
	} debugCmd;
	
	map<string, Debuggable*> debuggables;
	CommandController& commandController;
	MSXCPU* cpu;
};

} // namespace openmsx

