// $Id$

#ifndef __CLICOMMOUTPUT_HH__
#define __CLICOMMOUTPUT_HH__

#include <map>
#include <string>
#include "Command.hh"
#include "EventListener.hh"

using std::map;
using std::string;

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
	
	void log(LogLevel level, const string& message);
	void reply(ReplyStatus status, const string& message);
	void update(UpdateType type, const string& name, const string& value);

	// convient methods
	void printInfo(const string& message) {
		log(INFO, message);
	}
	void printWarning(const string& message) {
		log(WARNING, message);
	}

private:
	CliCommOutput();
	~CliCommOutput();
	
	class UpdateCmd : public SimpleCommand {
	public:
		UpdateCmd(CliCommOutput& parent);
		virtual string execute(const vector<string>& tokens);
		virtual string help(const vector<string>& tokens) const;
		virtual void tabCompletion(vector<string>& tokens) const;
	private:
		CliCommOutput& parent;
	} updateCmd;

	// EventListener
	virtual bool signalEvent(const Event& event);

	bool xmlOutput;
	bool updateEnabled[NUM_UPDATES];
	map<string, string> prevValues[NUM_UPDATES];
	CommandController& commandController;
};

} // namespace openmsx

#endif
