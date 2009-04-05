// $Id$

#ifndef CLICONNECTION_HH
#define CLICONNECTION_HH

#include "Thread.hh"
#include "Semaphore.hh"
#include "EventListener.hh"
#include "Socket.hh"
#include "CliComm.hh"
#include <libxml/parser.h>
#include <string>

namespace openmsx {

class CommandController;
class EventDistributor;

class CliConnection : private EventListener, protected Runnable
{
public:
	virtual ~CliConnection();

	virtual void output(const std::string& message) = 0;

	void setUpdateEnable(CliComm::UpdateType type, bool value);
	bool getUpdateEnable(CliComm::UpdateType type) const;

protected:
	CliConnection(CommandController& commandController,
	              EventDistributor& eventDistributor);

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

	void run();

	virtual void beforeConnection() = 0;
	virtual void connection() = 0;
	virtual void close() = 0;

	void startOutput();
	void endOutput();

	xmlParserCtxt* parser_context;
	Thread thread; // TODO: Possible to make this private?

private:
	void execute(const std::string& command);

	// EventListener
	virtual bool signalEvent(shared_ptr<const Event> event);

	enum State {
		START, TAG_OPENMSX, TAG_COMMAND, END
	};
	struct ParseState {
		State state;
		unsigned unknownLevel;
		std::string content;
		CliConnection* object;
	};

	static void cb_start_element(void* user_data, const xmlChar* name,
	                             const xmlChar** attrs);
	static void cb_end_element(void* user_data, const xmlChar* name);
	static void cb_text(void* user_data, const xmlChar* chars, int len);

	xmlSAXHandler sax_handler;
	ParseState user_data;

	CommandController& commandController;
	EventDistributor& eventDistributor;

	bool updateEnabled[CliComm::NUM_UPDATES];
};

class StdioConnection : public CliConnection
{
public:
	StdioConnection(CommandController& commandController,
	                EventDistributor& eventDistributor);
	virtual ~StdioConnection();

	virtual void output(const std::string& message);

private:
	virtual void close();
	virtual void beforeConnection();
	virtual void connection();

	bool ok;
};

#ifdef _WIN32
class PipeConnection : public CliConnection
{
public:
	PipeConnection(CommandController& commandController,
	               EventDistributor& eventDistributor,
	               const std::string& name);
	virtual ~PipeConnection();

	virtual void output(const std::string& message);

private:
	virtual void close();
	virtual void beforeConnection();
	virtual void connection();

	HANDLE pipeHandle;
};
#endif

class SocketConnection : public CliConnection
{
public:
	SocketConnection(CommandController& commandController,
	                 EventDistributor& eventDistributor,
	                 SOCKET sd);
	virtual ~SocketConnection();

	virtual void output(const std::string& message);

private:
	virtual void close();
	virtual void beforeConnection();
	virtual void connection();

	Semaphore sem;
	SOCKET sd;
};

} // namespace openmsx

#endif
