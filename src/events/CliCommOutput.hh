// $Id$

#ifndef CLICOMMOUTPUT_HH
#define CLICOMMOUTPUT_HH

#include <map>
#include <string>
#include "Command.hh"
#include "EventListener.hh"

namespace openmsx {

class CommandController;

class CliCommOutput : private EventListener
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
		SETTING,
		PLUG,
		UNPLUG,
		MEDIA,
		STATUS,
		NUM_UPDATES // must be last
	};
	
	static CliCommOutput& instance();
	void enableXMLOutput();
	
	void log(LogLevel level, const std::string& message);
	void reply(ReplyStatus status, const std::string& message);
	void update(UpdateType type, const std::string& name, const std::string& value);

	// convient methods
	void printInfo(const std::string& message) {
		log(INFO, message);
	}
	void printWarning(const std::string& message) {
		log(WARNING, message);
	}

private:
	CliCommOutput();
	~CliCommOutput();
	
	class UpdateCmd : public SimpleCommand {
	public:
		UpdateCmd(CliCommOutput& parent);
		virtual std::string execute(const std::vector<std::string>& tokens);
		virtual std::string help(const std::vector<std::string>& tokens) const;
		virtual void tabCompletion(std::vector<std::string>& tokens) const;
	private:
		CliCommOutput& parent;
	} updateCmd;

	// EventListener
	virtual bool signalEvent(const Event& event);

	bool xmlOutput;
	bool updateEnabled[NUM_UPDATES];
	std::map<std::string, std::string> prevValues[NUM_UPDATES];
	CommandController& commandController;
};

} // namespace openmsx

#endif
