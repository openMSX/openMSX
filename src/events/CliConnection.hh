// $Id$

#ifndef CLICONNECTION_HH
#define CLICONNECTION_HH

#include "Thread.hh"
#include "Schedulable.hh"
#include "Semaphore.hh"
#include "Socket.hh"
#include "CliComm.hh"
#include <libxml/parser.h>
#include <deque>
#include <string>

namespace openmsx {

class Scheduler;
class CommandController;

class CliConnection : private Schedulable, protected Runnable
{
public:
	virtual ~CliConnection();

	virtual void output(const std::string& message) = 0;

	void setUpdateEnable(CliComm::UpdateType type, bool value);
	bool getUpdateEnable(CliComm::UpdateType type) const;

protected:
	CliConnection(Scheduler& scheduler,
	              CommandController& commandController);

	/** Starts this connection by writing the opening tag
	  * and starting the listener thread.
	  * Subclasses should call this method at the end of their constructor.
	  */
	void start();

	/** End this connection by sending the closing tag
	  * and then closing the stream.
	  * Subclasses should call this method at the start of their destructor.
	  */
	void end();

	virtual void run() = 0;
	virtual void close() = 0;

	xmlParserCtxt* parser_context;
	Thread thread; // TODO: Possible to make this private?

private:
	void execute(const std::string& command);
	virtual void executeUntil(const EmuTime& time, int userData);
	virtual const std::string& schedName() const;

	enum State {
		START, TAG_OPENMSX, TAG_COMMAND, END
	};
	struct ParseState {
		State state;
		unsigned unknownLevel;
		std::string content;
		CliConnection* object;
	};

	static void cb_start_element(ParseState* user_data, const xmlChar* name,
				     const xmlChar** attrs);
	static void cb_end_element(ParseState* user_data, const xmlChar* name);
	static void cb_text(ParseState* user_data, const xmlChar* chars, int len);

	xmlSAXHandler sax_handler;
	ParseState user_data;
	Semaphore lock;
	std::deque<std::string> cmds;

	bool updateEnabled[CliComm::NUM_UPDATES];

	CommandController& commandController;
};

class StdioConnection : public CliConnection
{
public:
	StdioConnection(Scheduler& scheduler,
	                CommandController& commandController);
	virtual ~StdioConnection();

	virtual void output(const std::string& message);

private:
	virtual void close();
	virtual void run();

	bool ok;
};

#ifdef _WIN32
class PipeConnection : public CliConnection
{
public:
	PipeConnection(Scheduler& scheduler,
	               CommandController& commandController,
	               const std::string& name);
	virtual ~PipeConnection();

	virtual void output(const std::string& message);

private:
	virtual void close();
	virtual void run();

	HANDLE pipeHandle;
};
#endif

class SocketConnection : public CliConnection
{
public:
	SocketConnection(Scheduler& scheduler,
	                 CommandController& commandController,
	                 SOCKET sd);
	virtual ~SocketConnection();

	virtual void output(const std::string& message);

private:
	virtual void close();
	virtual void run();

	SOCKET sd;
};

} // namespace openmsx

#endif
