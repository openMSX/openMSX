// $Id$

#ifndef __CLICOMMINPUT_HH__
#define __CLICOMMINPUT_HH__

#include <deque>
#include <string>
#include <libxml/parser.h>
#include "Thread.hh"
#include "Schedulable.hh"
#include "Semaphore.hh"
#include "CommandLineParser.hh"

using std::deque;
using std::string;


namespace openmsx {

class CliCommInput : private Runnable, private Schedulable
{
public:
	CliCommInput(CommandLineParser::ControlType type, string arguments);
	virtual ~CliCommInput();

private:
	void execute(const string& command);
	virtual void executeUntil(const EmuTime& time, int userData) throw();
	virtual const string& schedName() const;
	virtual void run() throw();

	deque<string> cmds;
	Semaphore lock;
	Thread thread;
	CommandLineParser::ControlType ioType;
	string ioArguments;
	

	enum State {
		START, TAG_OPENMSX, TAG_COMMAND
	};
	struct ParseState {
		ParseState();
		State state;
		string content;
		CliCommInput* object;
	};

	static ParseState user_data;

	static void cb_start_element(ParseState* user_data, const xmlChar* name,
	                             const xmlChar** attrs);
	static void cb_end_element(ParseState* user_data, const xmlChar* name);
	static void cb_text(ParseState* user_data, const xmlChar* chars, int len);
};

} // namespace openmsx

#endif // __CLICOMMINPUT_HH__
