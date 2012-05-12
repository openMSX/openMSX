// $Id$

#ifndef CLICONNECTION_HH
#define CLICONNECTION_HH

#include "CliListener.hh"
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

class CliConnection : public CliListener, private EventListener,
                      protected Runnable
{
public:
	virtual ~CliConnection();

	void setUpdateEnable(CliComm::UpdateType type, bool value);
	bool getUpdateEnable(CliComm::UpdateType type) const;

protected:
	CliConnection(CommandController& commandController,
	              EventDistributor& eventDistributor);

	virtual void output(const std::string& message) = 0;

	/** Starts the helper thread.
	  * Subclasses should call this method at the end of their constructor.
	  * Subclasses should themself send the opening tag (startOutput()).
	  */
	void start();

	/** End this connection by sending the closing tag
	  * and then closing the stream.
	  * Subclasses should call this method at the start of their destructor.
	  */
	void end();

	/** Close the connection. After this method is called, calls to
	  * output() should be ignored.
	  */
	virtual void close() = 0;

	/** Send opening XML tag, should be called exactly once by a subclass
	  * shortly after opening a connection. Cannot be implemented in the
	  * base class because some subclasses (want to send data before this
	  * tag).
	  */
	void startOutput();

	xmlParserCtxt* parser_context;
	Thread thread; // TODO: Possible to make this private?

private:
	void execute(const std::string& command);

	// CliListener
	virtual void log(CliComm::LogLevel level, const std::string& message);
	virtual void update(CliComm::UpdateType type, const std::string& machine,
	                    const std::string& name, const std::string& value);

	// EventListener
	virtual int signalEvent(const shared_ptr<const Event>& event);

	enum State {
		START, TAG_OPENMSX, TAG_COMMAND, END
	};
	struct ParseState {
		State state;
		unsigned unknownLevel;
		std::string content;
		CliConnection* object;
	};

	static void cb_start_element(void* user_data, const xmlChar* localname,
	                             const xmlChar* prefix, const xmlChar* uri,
	                             int nb_namespaces, const xmlChar** namespaces,
	                             int nb_attributes, int nb_defaulted,
	                             const xmlChar** attrs);
	static void cb_end_element(void* user_data, const xmlChar* localname,
	                           const xmlChar* prefix, const xmlChar* uri);
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
	virtual void run();

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
	virtual void run();

	HANDLE pipeHandle;
	HANDLE shutdownEvent;
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
	virtual void run();

	Semaphore sem;
	SOCKET sd;
	bool established;
};

} // namespace openmsx

#endif
