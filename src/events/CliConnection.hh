#ifndef CLICONNECTION_HH
#define CLICONNECTION_HH

#include "CliListener.hh"
#include "Thread.hh"
#include "Semaphore.hh"
#include "EventListener.hh"
#include "Socket.hh"
#include "CliComm.hh"
#include "AdhocCliCommParser.hh"
#include <string>

namespace openmsx {

class CommandController;
class EventDistributor;

class CliConnection : public CliListener, private EventListener,
                      protected Runnable
{
public:
	virtual ~CliConnection();

	void setUpdateEnable(CliComm::UpdateType type, bool value) {
		updateEnabled[type] = value;
	}
	bool getUpdateEnable(CliComm::UpdateType type) const {
		return updateEnabled[type];
	}

	/** Starts the helper thread.
	  * Called when this CliConnection is added to GlobalCliComm (and
	  * after it's allowed to respond to external commands).
	  * Subclasses should themself send the opening tag (startOutput()).
	  */
	void start();

protected:
	CliConnection(CommandController& commandController,
	              EventDistributor& eventDistributor);

	virtual void output(string_ref message) = 0;

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

	AdhocCliCommParser parser;
	Thread thread; // TODO: Possible to make this private?

private:
	void execute(const std::string& command);

	// CliListener
	virtual void log(CliComm::LogLevel level, string_ref message);
	virtual void update(CliComm::UpdateType type, string_ref machine,
	                    string_ref name, string_ref value);

	// EventListener
	virtual int signalEvent(const std::shared_ptr<const Event>& event);

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

	virtual void output(string_ref message);

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
	               string_ref name);
	virtual ~PipeConnection();

	virtual void output(string_ref message);

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

	virtual void output(string_ref message);

private:
	virtual void close();
	virtual void run();

	Semaphore sem;
	SOCKET sd;
	bool established;
};

} // namespace openmsx

#endif
