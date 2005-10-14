// $Id$

#ifndef CLICOMM_HH
#define CLICOMM_HH

#include "CommandLineParser.hh"
#include "Command.hh"
#include "EventListener.hh"
#include <map>
#include <string>

namespace openmsx {

class Scheduler;
class EventDistributor;
class CommandController;
class CliConnection;

class CliComm : private EventListener
{
public:
	enum LogLevel {
		INFO,
		WARNING
	};
	enum ReplyStatus {
		OK,
		NOK
	};
	enum UpdateType {
		LED,
		BREAK,
		RESUME,
		SETTING,
		PLUG,
		UNPLUG,
		MEDIA,
		STATUS,
		NUM_UPDATES // must be last
	};

	CliComm(Scheduler& scheduler, CommandController& commandController,
	        EventDistributor& eventDistributor);
	virtual ~CliComm();

	void startInput(CommandLineParser::ControlType type,
	                const std::string& arguments);

	void log(LogLevel level, const std::string& message);
	void update(UpdateType type, const std::string& name, const std::string& value);

	// convenience methods
	void printInfo(const std::string& message) {
		log(INFO, message);
	}
	void printWarning(const std::string& message) {
		log(WARNING, message);
	}

private:
	// EventListener
	virtual void signalEvent(const Event& event);

	class UpdateCmd : public SimpleCommand {
	public:
		UpdateCmd(CommandController& commandController,
		          CliComm& cliComm);
		virtual std::string execute(const std::vector<std::string>& tokens);
		virtual std::string help(const std::vector<std::string>& tokens) const;
		virtual void tabCompletion(std::vector<std::string>& tokens) const;
	private:
		CliConnection& getConnection();
		CliComm& cliComm;
	} updateCmd;

	std::map<std::string, std::string> prevValues[NUM_UPDATES];

	Scheduler& scheduler;
	CommandController& commandController;
	EventDistributor& eventDistributor;

	bool xmlOutput;
	typedef std::vector<CliConnection*> Connections;
	Connections connections;
	friend class CliServer;

};

} // namespace openmsx

#endif
