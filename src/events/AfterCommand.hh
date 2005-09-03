// $Id$

#ifndef AFTERCOMMAND_HH
#define AFTERCOMMAND_HH

#include "Command.hh"
#include "EventListener.hh"
#include "Schedulable.hh"
#include "Event.hh"
#include <map>

namespace openmsx {

class EventDistributor;
class CommandController;

class AfterCommand : public SimpleCommand, private EventListener
{
public:
	AfterCommand(Scheduler& scheduler,
	             EventDistributor& eventDistributor,
	             CommandController& commandController);
	virtual ~AfterCommand();

	virtual std::string execute(const std::vector<std::string>& tokens);
	virtual std::string help(const std::vector<std::string>& tokens) const;
	virtual void tabCompletion(std::vector<std::string>& tokens) const;

private:
	std::string afterTime(const std::vector<std::string>& tokens);
	std::string afterIdle(const std::vector<std::string>& tokens);
	std::string afterInfo(const std::vector<std::string>& tokens);
	std::string afterCancel(const std::vector<std::string>& tokens);
	template<EventType T> std::string afterEvent(
		const std::vector<std::string>& tokens);
	template<EventType T> void executeEvents();

	// EventListener
	virtual void signalEvent(const Event& event);


	class AfterCmd {
	public:
		virtual ~AfterCmd();
		const std::string& getCommand() const;
		const std::string& getId() const;
		virtual const std::string& getType() const = 0;
		void execute();
	protected:
		AfterCmd(CommandController& commandController,
		         const std::string& command);
	private:
		CommandController& commandController;
		std::string command;
		std::string id;
		static unsigned lastAfterId;
	};

	class AfterTimedCmd : public AfterCmd, private Schedulable {
	public:
		virtual ~AfterTimedCmd();
		double getTime() const;
		void reschedule();
	protected:
		AfterTimedCmd(Scheduler& scheduler,
		              CommandController& commandController,
		              const std::string& command, double time);
	private:
		virtual void executeUntil(const EmuTime& time, int userData);
		virtual const std::string& schedName() const;

		double time;
	};

	class AfterTimeCmd : public AfterTimedCmd {
	public:
		AfterTimeCmd(Scheduler& scheduler,
		             CommandController& commandController,
		             const std::string& command, double time);
		virtual const std::string& getType() const;
	};

	class AfterIdleCmd : public AfterTimedCmd {
	public:
		AfterIdleCmd(Scheduler& scheduler,
		             CommandController& commandController,
		             const std::string& command, double time);
		virtual const std::string& getType() const;
	};

	template<EventType T>
	class AfterEventCmd : public AfterCmd {
	public:
		AfterEventCmd(CommandController& commandController,
		              const std::string& command);
		virtual const std::string& getType() const;
	};

	typedef std::map<std::string, AfterCmd*> AfterCmdMap;
	static AfterCmdMap afterCmds;

	Scheduler& scheduler;
	EventDistributor& eventDistributor;
	CommandController& commandController;
};

} // namespace openmsx

#endif
