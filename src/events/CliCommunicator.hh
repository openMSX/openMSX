// $Id$

#ifndef __CLICOMMUNICATOR_HH__
#define __CLICOMMUNICATOR_HH__

#include <deque>
#include <string>
#include <libxml/parser.h>
#include "Thread.hh"
#include "Schedulable.hh"
#include "Semaphore.hh"

using std::deque;
using std::string;

namespace openmsx {

class CliCommunicator : public Runnable, private Schedulable
{
public:
	static CliCommunicator& instance();
	virtual void run();

	void printInfo(const string& message);
	void printWarning(const string& message);
	void printUpdate(const string& message);

private:
	CliCommunicator();
	virtual ~CliCommunicator();

	void execute(const string& command);
	virtual void executeUntilEmuTime(const EmuTime& time, int userData);
	virtual const string& schedName() const;
	
	deque<string> cmds;
	Semaphore lock;
	
	enum State {
		START, TAG_OPENMSX, TAG_COMMAND
	};
	struct ParseState {
		State state;
		string content;
	};
	static ParseState user_data;
	
	static void cb_start_element(ParseState* user_data, const xmlChar* name,
                                     const xmlChar** attrs);
	static void cb_end_element(ParseState* user_data, const xmlChar* name);
	static void cb_text(ParseState* user_data, const xmlChar* chars, int len);
};

} // namespace openmsx

#endif
