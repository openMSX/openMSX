// $Id$

#ifndef __AFTERCOMMAND_HH__
#define __AFTERCOMMAND_HH__

#include <map>
#include "Command.hh"
#include "EventListener.hh"
#include "Schedulable.hh"

using std::map;

namespace openmsx {

class AfterCommand : public Command, private EventListener
{
public:
	AfterCommand();
	virtual ~AfterCommand();
	
	virtual string execute(const vector<string>& tokens)
		throw(CommandException);
	virtual string help(const vector<string>& tokens) const
		throw();
	virtual void tabCompletion(const vector<string>& tokens) const
		throw();

private:
	enum AfterType { TIME, IDLE };
	
	string afterTime(const vector<string>& tokens);
	string afterIdle(const vector<string>& tokens);
	string afterInfo(const vector<string>& tokens);
	string afterCancel(const vector<string>& tokens);
	string afterNew(const vector<string>& tokens, AfterType type);

	// EventListener
	virtual bool signalEvent(const SDL_Event& event) throw();
	
	struct AfterCmd : public Schedulable {
		AfterCmd(AfterCommand& parent);
		virtual ~AfterCmd();
		unsigned id;
		AfterType type;
		float time;
		string command;
	private:
		virtual void executeUntil(const EmuTime& time, int userData)
			throw();
		virtual const string& schedName() const;
		AfterCommand& parent;
	};
	map<unsigned, AfterCmd*> afterCmds;
	unsigned lastAfterId;
};

} // namespace openmsx

#endif
