// $Id$

#ifndef __ALIASCOMMANDS_HH__
#define __ALIASCOMMANDS_HH__

#include <map>
#include <set>
#include "Command.hh"

using std::map;
using std::set;

namespace openmsx {

class CommandController;

class AliasCommands
{
public:
	AliasCommands();
	~AliasCommands();

private:
	void getAliases(set<string>& result) const;

	class Alias;
	map<string, Alias*> aliasses;

	// Commands
	class Alias : public Command {
	public:
		Alias(const string& name, const string& definition);
		virtual ~Alias();

		const string& getName() const;
		const string& getDefinition() const;

		virtual string execute(const vector<string> &tokens)
			throw(CommandException);
		virtual string help(const vector<string> &tokens) const
			throw();
	private:
		const string name;
		const string definition;
	};
	
	class AliasCmd : public Command {
	public:
		AliasCmd(AliasCommands& parent);
		virtual string execute(const vector<string> &tokens)
			throw(CommandException);
		virtual string help(const vector<string> &tokens) const
			throw();
		virtual void tabCompletion(vector<string> &tokens) const
			throw();
	private:
		AliasCommands& parent;
	} aliasCmd;
	friend class AliasCmd;

	class UnaliasCmd : public Command {
	public:
		UnaliasCmd(AliasCommands& parent);
		virtual string execute(const vector<string> &tokens)
			throw(CommandException);
		virtual string help(const vector<string> &tokens) const
			throw();
		virtual void tabCompletion(vector<string> &tokens) const
			throw();
	private:
		AliasCommands& parent;
	} unaliasCmd;
	friend class UnaliasCmd;
};

} // namespace openmsx

#endif
