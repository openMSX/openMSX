// $Id$

#ifndef CLICONNECTION_HH
#define CLICONNECTION_HH

#include "Thread.hh"
#include "Schedulable.hh"
#include "Semaphore.hh"
#include "Socket.hh"
#include <libxml/parser.h>
#include <deque>
#include <string>

namespace openmsx {

class CliConnection : private Schedulable
{
public:
	virtual ~CliConnection();

	virtual void output(const std::string& message) = 0;
	virtual void close() = 0;

protected:
	CliConnection();

	xmlParserCtxt* parser_context;

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
};

class StdioConnection : public CliConnection, private Runnable
{
public:
	StdioConnection();
	virtual ~StdioConnection();

	virtual void output(const std::string& message);
	virtual void close();

private:
	virtual void run();

	Thread thread;
	bool ok;
};

#ifdef _WIN32
class PipeConnection : public CliConnection, private Runnable
{
public:
	PipeConnection(const std::string& name);
	virtual ~PipeConnection();

	virtual void output(const std::string& message);
	virtual void close();

private:
	virtual void run();

	Thread thread;
	HANDLE pipeHandle;
};
#endif

class SocketConnection : public CliConnection, private Runnable
{
public:
	SocketConnection(SOCKET sd);
	virtual ~SocketConnection();

	virtual void output(const std::string& message);
	virtual void close();

private:
	virtual void run();

	Thread thread;
	SOCKET sd;
};

} // namespace openmsx

#endif
