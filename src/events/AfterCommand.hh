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
	virtual void tabCompletion(vector<string>& tokens) const
		throw();

private:
	string afterTime(const vector<string>& tokens);
	string afterIdle(const vector<string>& tokens);
	string afterInfo(const vector<string>& tokens);
	string afterCancel(const vector<string>& tokens);
	template<EventType T> string afterEvent(const vector<string>& tokens);
	template<EventType T> void executeEvents();

	// EventListener
	virtual bool signalEvent(const Event& event) throw();

	
	class AfterCmd {
	public:
		virtual ~AfterCmd();
		const string& getCommand() const;
		const string& getId() const;
		virtual const string& getType() const = 0;
		void execute() throw();
	protected:
		AfterCmd(const string& command);
	private:
		string command;
		string id;
		static unsigned lastAfterId;
	};

	class AfterTimedCmd : public AfterCmd, private Schedulable {
	public:
		virtual ~AfterTimedCmd();
		float getTime() const;
		void reschedule();
	protected:
		AfterTimedCmd(const string& command, float time);
	private:
		virtual void executeUntil(const EmuTime& time, int userData)
			throw();
		virtual const string& schedName() const;
		
		float time;
	};

	class AfterTimeCmd : public AfterTimedCmd {
	public:
		AfterTimeCmd(const string& command, float time);
		virtual const string& getType() const;
	};
	
	class AfterIdleCmd : public AfterTimedCmd {
	public:
		AfterIdleCmd(const string& command, float time);
		virtual const string& getType() const;
	};
	
	template<EventType T>
	class AfterEventCmd : public AfterCmd {
	public:
		AfterEventCmd(const string& command);
		virtual const string& getType() const;
	};
	
	typedef map<string, AfterCmd*> AfterCmdMap;
	static AfterCmdMap afterCmds;
};

} // namespace openmsx

#endif
